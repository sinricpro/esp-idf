
## API Reference

### Core API

#### Initialization

```c
esp_err_t sinricpro_init(const sinricpro_config_t *config);
esp_err_t sinricpro_start(void);
esp_err_t sinricpro_stop(void);
esp_err_t sinricpro_deinit(void);
bool sinricpro_is_connected(void);
uint32_t sinricpro_get_timestamp(void);
const char* sinricpro_get_version(void);
```

#### Configuration Structure

```c
typedef struct {
    const char *app_key;                /* Required: APP_KEY from portal */
    const char *app_secret;             /* Required: APP_SECRET from portal */
    const char *server_url;             /* Optional: NULL = use default */
    uint16_t server_port;               /* Optional: 0 = use default */
    bool auto_reconnect;                /* Enable auto-reconnection */
    uint32_t reconnect_interval_ms;     /* Reconnection interval */
    uint32_t heartbeat_interval_ms;     /* Heartbeat interval */
} sinricpro_config_t;
```

### Switch Device API

#### Device Management

```c
sinricpro_device_handle_t sinricpro_switch_create(const char *device_id);
esp_err_t sinricpro_switch_delete(sinricpro_device_handle_t device);
```

#### Callbacks

```c
/* PowerState callback signature */
typedef bool (*sinricpro_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/* Setting callback signature */
typedef bool (*sinricpro_setting_callback_t)(
    const char *device_id,
    const char *setting_id,
    const char *value,
    void *user_data
);

/* Register callbacks */
esp_err_t sinricpro_switch_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_power_state_callback_t callback,
    void *user_data
);

esp_err_t sinricpro_switch_on_setting(
    sinricpro_device_handle_t device,
    sinricpro_setting_callback_t callback,
    void *user_data
);
```

#### Events

```c
/* Send PowerState event */
esp_err_t sinricpro_switch_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause  /* SINRICPRO_CAUSE_PHYSICAL_INTERACTION, etc. */
);

/* Send push notification */
esp_err_t sinricpro_switch_send_notification(
    sinricpro_device_handle_t device,
    const char *message
);
```

### Event System

```c
/* Register for connection events */
esp_event_handler_register(SINRICPRO_EVENT, SINRICPRO_EVENT_CONNECTED,
                             &event_handler, NULL);

/* Event IDs */
SINRICPRO_EVENT_CONNECTED
SINRICPRO_EVENT_DISCONNECTED
SINRICPRO_EVENT_ERROR
```

### Constants

```c
/* Cause types */
SINRICPRO_CAUSE_PHYSICAL_INTERACTION  /* Physical button press */
SINRICPRO_CAUSE_PERIODIC_POLL         /* Periodic sensor update */
SINRICPRO_CAUSE_VOICE_INTERACTION     /* Voice command */
SINRICPRO_CAUSE_APP_INTERACTION       /* Mobile app control */
```

## Configuration (Kconfig)

Access via `idf.py menuconfig` → `Component config` → `SinricPro Configuration`:

- **Server URL** - SinricPro server URL (default: ws.sinric.pro)
- **Server Port** - Server port (default: 80)
- **WebSocket Task Stack Size** - Stack size for WebSocket task
- **WebSocket Task Priority** - Priority of WebSocket task
- **Callback Task Stack Size** - Stack size for callback task
- **Enable Debug Logging** - Verbose logging for troubleshooting
- **Event Queue Size** - Maximum queued events
- **Message Queue Size** - Maximum queued messages
- **Auto-reconnection** - Enable/disable auto-reconnection
- **Reconnection Interval** - Time between reconnection attempts
- **Max Devices** - Maximum number of registered devices

## Examples

### Switch Example

Complete example with WiFi setup, button control, and voice control.

```bash
cd components/sinricpro/examples/switch
idf.py build flash monitor
```

See [examples/switch/README.md](examples/switch/README.md) for details.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  Application Layer                      │
│  (Your code, callbacks, device initialization)          │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│              SinricPro Device Layer                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │   Switch    │  │    Light    │  │     Fan     │     │
│  └─────────────┘  └─────────────┘  └─────────────┘     │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│            SinricPro Capability Layer                   │
│  ┌──────────────────┐  ┌────────────────────────┐       │
│  │ PowerState       │  │ Setting                │       │
│  │ Controller       │  │ Controller             │       │
│  └──────────────────┘  └────────────────────────┘       │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│              SinricPro Core Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  WebSocket   │  │  Signature   │  │EventLimiter  │  │
│  │   Client     │  │  (HMAC-256)  │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐                     │
│  │   Message    │  │   Device     │                     │
│  │    Queue     │  │   Registry   │                     │
│  └──────────────┘  └──────────────┘                     │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│                 ESP-IDF Layer                           │
│  esp_websocket_client | mbedtls | esp_event | FreeRTOS  │
└─────────────────────────────────────────────────────────┘
```

## Error Handling

```c
esp_err_t ret = sinricpro_start();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start: %s", esp_err_to_name(ret));
}

/* Error codes */
SINRICPRO_ERR_INVALID_CONFIG    /* Invalid configuration */
SINRICPRO_ERR_NOT_INITIALIZED   /* Not initialized */
SINRICPRO_ERR_NOT_CONNECTED     /* Not connected to server */
SINRICPRO_ERR_RATE_LIMITED      /* Event rate limited */
SINRICPRO_ERR_QUEUE_FULL        /* Message queue full */
```

## Troubleshooting

### Connection Issues

**Cannot connect to server:**
1. Check APP_KEY and APP_SECRET are correct
2. Verify DEVICE_ID is exactly 24 hex characters
3. Ensure internet connectivity
4. Enable debug logging in menuconfig

**Frequent disconnections:**
1. Check WiFi signal strength
2. Increase reconnection interval
3. Monitor ESP_LOG output for errors

### Callback Issues

**Callback not called:**
1. Ensure callback is registered before `sinricpro_start()`
2. Check device ID matches SinricPro portal
3. Verify Alexa/Google Home skill is linked

**Callback crashes:**
1. Ensure callback completes quickly (< 100ms)
2. Don't call blocking functions in callbacks
3. Use FreeRTOS queues for heavy processing

### Event Issues

**Events not sent:**
1. Check `sinricpro_is_connected()` returns true
2. Verify return value of `send_event()` functions
3. Check for rate limiting (max 1 event/second)

**Rate limited:**
- Events are limited to 1 per second for state changes
- Check return value for `SINRICPRO_ERR_RATE_LIMITED`
- Wait before sending next event

## Performance

- **Memory Usage:** ~40KB heap (includes WebSocket buffer)
- **Task Stack:** 8KB WebSocket + 4KB callback task
- **Event Rate:** Max 1 state event/second, 1 sensor event/60 seconds
- **Latency:** < 500ms for voice commands

## Debugging

Enable debug logging in menuconfig or programmatically:

```c
esp_log_level_set("sinricpro_core", ESP_LOG_DEBUG);
esp_log_level_set("sinricpro_websocket", ESP_LOG_DEBUG);
```

## Thread Safety

All public APIs are thread-safe. Internal synchronization uses:
- FreeRTOS mutexes for shared state
- FreeRTOS queues for message passing
- Event groups for connection status
