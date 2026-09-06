/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_mdns.h"

#if defined(CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL) && \
    !defined(CONFIG_SINRICPRO_LOCAL_CONTROL_NO_MDNS)

#include "sinricpro.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "mdns.h"

static const char *TAG = "sinricpro_mdns";

#define SINRICPRO_MDNS_SERVICE "_sinricpro"
#define SINRICPRO_MDNS_PROTO   "_udp"

static struct {
    bool announced;
    char *device_ids;
    char host_name[32];
} mdns_state = {0};

/**
 * @brief Build the announced host name: sinricpro-<sta mac, lowercase, no separators>
 */
static void sinricpro_mdns_build_host_name(void)
{
    uint8_t mac[6] = {0};

    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGW(TAG, "Could not read station MAC, host name will not be unique");
    }

    snprintf(mdns_state.host_name, sizeof(mdns_state.host_name),
             "sinricpro-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

esp_err_t sinricpro_mdns_start(const char *device_ids)
{
    if (mdns_state.announced) {
        sinricpro_mdns_update(device_ids);
        return ESP_OK;
    }

    /* Idempotent: returns ESP_OK if the application already started mDNS. */
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s, device not discoverable on the LAN",
                 esp_err_to_name(ret));
        return ret;
    }

    sinricpro_mdns_build_host_name();

    ret = mdns_hostname_set(mdns_state.host_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns_hostname_set(%s) failed: %s",
                 mdns_state.host_name, esp_err_to_name(ret));
        return ret;
    }

    free(mdns_state.device_ids);
    mdns_state.device_ids = strdup(device_ids ? device_ids : "");
    if (mdns_state.device_ids == NULL) {
        return ESP_ERR_NO_MEM;
    }

    mdns_txt_item_t txt[] = {
        {"deviceIds", mdns_state.device_ids},
        {"sdk",       SINRICPRO_VERSION},
        {"udp",       "1"},
    };

    ret = mdns_service_add(mdns_state.host_name, SINRICPRO_MDNS_SERVICE,
                            SINRICPRO_MDNS_PROTO, CONFIG_SINRICPRO_UDP_PORT,
                            txt, sizeof(txt) / sizeof(txt[0]));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns_service_add failed: %s", esp_err_to_name(ret));
        free(mdns_state.device_ids);
        mdns_state.device_ids = NULL;
        return ret;
    }

    mdns_state.announced = true;

    ESP_LOGI(TAG, "Announced %s.%s.local. as %s.local. port=%d deviceIds=%s",
             SINRICPRO_MDNS_SERVICE, SINRICPRO_MDNS_PROTO, mdns_state.host_name,
             CONFIG_SINRICPRO_UDP_PORT, mdns_state.device_ids);

    return ESP_OK;
}

void sinricpro_mdns_update(const char *device_ids)
{
    const char *ids = device_ids ? device_ids : "";

    if (!mdns_state.announced ||
        (mdns_state.device_ids && strcmp(mdns_state.device_ids, ids) == 0)) {
        return;
    }

    char *copy = strdup(ids);
    if (copy == NULL) {
        return;
    }

    free(mdns_state.device_ids);
    mdns_state.device_ids = copy;

    esp_err_t ret = mdns_service_txt_item_set(SINRICPRO_MDNS_SERVICE,
                                               SINRICPRO_MDNS_PROTO,
                                               "deviceIds",
                                               mdns_state.device_ids);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns deviceIds update failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "mDNS deviceIds updated: %s", mdns_state.device_ids);
}

void sinricpro_mdns_stop(void)
{
    if (!mdns_state.announced) {
        return;
    }

    mdns_service_remove(SINRICPRO_MDNS_SERVICE, SINRICPRO_MDNS_PROTO);
    mdns_state.announced = false;

    free(mdns_state.device_ids);
    mdns_state.device_ids = NULL;

    ESP_LOGI(TAG, "mDNS announcement withdrawn");
}

#else /* local control or mDNS disabled */

esp_err_t sinricpro_mdns_start(const char *device_ids)
{
    (void)device_ids;
    return ESP_OK;
}

void sinricpro_mdns_update(const char *device_ids)
{
    (void)device_ids;
}

void sinricpro_mdns_stop(void)
{
}

#endif
