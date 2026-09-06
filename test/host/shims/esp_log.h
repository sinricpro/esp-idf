/* Host-test shim: logging is not what these tests exercise. */
#ifndef SINRICPRO_HOST_SHIM_ESP_LOG_H
#define SINRICPRO_HOST_SHIM_ESP_LOG_H

#define ESP_LOGE(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGI(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGD(tag, ...) do { (void)(tag); } while (0)

#endif
