/* Host-test shim: sinricpro.h declares its event base; nothing under test posts events. */
#ifndef SINRICPRO_HOST_SHIM_ESP_EVENT_H
#define SINRICPRO_HOST_SHIM_ESP_EVENT_H

typedef const char *esp_event_base_t;

#define ESP_EVENT_DECLARE_BASE(id) extern esp_event_base_t const id

#endif
