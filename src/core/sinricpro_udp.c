/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_udp.h"

#ifdef CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

static const char *TAG = "sinricpro_udp";

/* A request is a signed JSON envelope; the app's largest is well under 1 KB. */
#define SINRICPRO_UDP_RX_BUFFER_SIZE 1600

/* Blocking recvfrom() timeout, so stop() is acted on promptly. */
#define SINRICPRO_UDP_RECV_TIMEOUT_MS 1000

/* Retry interval for binding / joining while the interface has no address. */
#define SINRICPRO_UDP_RETRY_MS 5000

static struct {
    int sock;
    bool running;
    volatile bool stop_requested;
    TaskHandle_t task;
    SemaphoreHandle_t sock_mutex;  /* serialises sendto against close() */
    sinricpro_udp_rx_cb_t on_receive;
    void *context;
} udp_state = {
    .sock = -1,
};

/**
 * @brief IPv4 address of the station interface, or INADDR_ANY if it has none
 *
 * The join is bound to the interface that actually carries the LAN; falling
 * back to INADDR_ANY lets lwIP pick the default netif rather than failing.
 */
static uint32_t sinricpro_udp_interface_addr(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;

    if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        return ip_info.ip.addr;
    }

    return htonl(INADDR_ANY);
}

/**
 * @brief Bind the socket and join the multicast group
 *
 * @return The socket fd, or -1 if local control could not be brought up
 */
static int sinricpro_udp_open(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        return -1;
    }

    int yes = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        ESP_LOGW(TAG, "SO_REUSEADDR failed: errno %d", errno);
    }

    struct timeval tv = {
        .tv_sec = SINRICPRO_UDP_RECV_TIMEOUT_MS / 1000,
        .tv_usec = (SINRICPRO_UDP_RECV_TIMEOUT_MS % 1000) * 1000,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Bound to INADDR_ANY, not to the group: unicast to this host on the same
     * port must be received too. */
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(CONFIG_SINRICPRO_UDP_PORT),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() to port %d failed: errno %d, local control unavailable",
                 CONFIG_SINRICPRO_UDP_PORT, errno);
        close(sock);
        return -1;
    }

    struct ip_mreq mreq = {
        .imr_multiaddr.s_addr = inet_addr(CONFIG_SINRICPRO_UDP_MULTICAST_IP),
        .imr_interface.s_addr = sinricpro_udp_interface_addr(),
    };

    /* A failed join leaves nothing answering the group and says nothing unless
     * the return value is checked. Log the outcome either way. */
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        ESP_LOGE(TAG, "IP_ADD_MEMBERSHIP %s failed: errno %d, local control unavailable",
                 CONFIG_SINRICPRO_UDP_MULTICAST_IP, errno);
        close(sock);
        return -1;
    }

    esp_ip4_addr_t iface = { .addr = mreq.imr_interface.s_addr };
    ESP_LOGI(TAG, "Local control listening on UDP %d, joined %s on " IPSTR,
             CONFIG_SINRICPRO_UDP_PORT, CONFIG_SINRICPRO_UDP_MULTICAST_IP,
             IP2STR(&iface));

    return sock;
}

static void sinricpro_udp_close(void)
{
    xSemaphoreTake(udp_state.sock_mutex, portMAX_DELAY);
    if (udp_state.sock >= 0) {
        close(udp_state.sock);
        udp_state.sock = -1;
    }
    udp_state.running = false;
    xSemaphoreGive(udp_state.sock_mutex);
}

static void sinricpro_udp_task(void *arg)
{
    char *buffer = malloc(SINRICPRO_UDP_RX_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate receive buffer, local control unavailable");
        udp_state.task = NULL;
        vTaskDelete(NULL);
        return;
    }

    while (!udp_state.stop_requested) {
        if (udp_state.sock < 0) {
            int sock = sinricpro_udp_open();
            if (sock < 0) {
                vTaskDelay(pdMS_TO_TICKS(SINRICPRO_UDP_RETRY_MS));
                continue;
            }
            xSemaphoreTake(udp_state.sock_mutex, portMAX_DELAY);
            udp_state.sock = sock;
            udp_state.running = true;
            xSemaphoreGive(udp_state.sock_mutex);
        }

        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        int len = recvfrom(udp_state.sock, buffer, SINRICPRO_UDP_RX_BUFFER_SIZE - 1, 0,
                           (struct sockaddr *)&peer, &peer_len);

        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;  /* recv timeout, loop so stop_requested is re-checked */
            }
            ESP_LOGW(TAG, "recvfrom() failed: errno %d, reopening socket", errno);
            sinricpro_udp_close();
            vTaskDelay(pdMS_TO_TICKS(SINRICPRO_UDP_RETRY_MS));
            continue;
        }

        if (len == 0) {
            continue;
        }

        buffer[len] = '\0';

        sinricpro_msg_origin_t origin = {
            .transport = SINRICPRO_TRANSPORT_UDP,
            .peer_addr = peer.sin_addr.s_addr,
            .peer_port = ntohs(peer.sin_port),
        };

        esp_ip4_addr_t peer_ip = { .addr = origin.peer_addr };
        ESP_LOGD(TAG, "Request from " IPSTR ":%u (%d bytes): %s",
                 IP2STR(&peer_ip), origin.peer_port, len, buffer);

        if (udp_state.on_receive) {
            udp_state.on_receive(buffer, (size_t)len, &origin, udp_state.context);
        }
    }

    free(buffer);
    sinricpro_udp_close();

    ESP_LOGI(TAG, "Local control listener stopped");
    udp_state.task = NULL;
    vTaskDelete(NULL);
}

esp_err_t sinricpro_udp_start(sinricpro_udp_rx_cb_t cb, void *context)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (udp_state.task != NULL) {
        ESP_LOGW(TAG, "Local control already started");
        return ESP_ERR_INVALID_STATE;
    }

    if (udp_state.sock_mutex == NULL) {
        udp_state.sock_mutex = xSemaphoreCreateMutex();
        if (udp_state.sock_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create socket mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    udp_state.on_receive = cb;
    udp_state.context = context;
    udp_state.stop_requested = false;

    BaseType_t ret = xTaskCreate(sinricpro_udp_task, "sinricpro_udp",
                                  CONFIG_SINRICPRO_UDP_TASK_STACK_SIZE, NULL,
                                  CONFIG_SINRICPRO_UDP_TASK_PRIORITY,
                                  &udp_state.task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create local control task");
        udp_state.task = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

void sinricpro_udp_stop(void)
{
    if (udp_state.task == NULL) {
        return;
    }

    udp_state.stop_requested = true;

    /* One recv timeout plus slack; the task frees its own resources. */
    for (int i = 0; i < 20 && udp_state.task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool sinricpro_udp_is_running(void)
{
    return udp_state.running;
}

esp_err_t sinricpro_udp_send(const char *message,
                              uint32_t peer_addr,
                              uint16_t peer_port)
{
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (peer_port == 0) {
        ESP_LOGW(TAG, "Message has no peer to answer, dropping");
        return ESP_ERR_INVALID_ARG;
    }

    if (udp_state.sock_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = peer_addr,
        .sin_port = htons(peer_port),
    };

    xSemaphoreTake(udp_state.sock_mutex, portMAX_DELAY);

    if (udp_state.sock < 0) {
        xSemaphoreGive(udp_state.sock_mutex);
        ESP_LOGW(TAG, "Local control socket is closed, dropping reply");
        return ESP_ERR_INVALID_STATE;
    }

    int sent = sendto(udp_state.sock, message, strlen(message), 0,
                      (struct sockaddr *)&dest, sizeof(dest));
    xSemaphoreGive(udp_state.sock_mutex);

    esp_ip4_addr_t peer_ip = { .addr = peer_addr };

    if (sent < 0) {
        ESP_LOGE(TAG, "Reply to " IPSTR ":%u failed: errno %d",
                 IP2STR(&peer_ip), peer_port, errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Reply to " IPSTR ":%u (%d bytes)", IP2STR(&peer_ip), peer_port, sent);

    return ESP_OK;
}

#endif /* CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL */
