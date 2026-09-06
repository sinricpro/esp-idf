# Changelog

## [1.3.0]

### Features

- feat: local control. The device answers signed SinricPro commands over the LAN
  (UDP 3333, multicast 224.9.9.9, unicast too), so it keeps responding to the app
  while the cloud is unreachable. Requests are dispatched through the same
  capability callbacks as cloud requests.
- feat: mDNS announcement of `_sinricpro._udp.local.` as `sinricpro-<mac>`, with
  TXT records `deviceIds`, `sdk` and `udp=1`, refreshed when the device list changes.
- feat: Kconfig gates `SINRICPRO_ENABLE_LOCAL_CONTROL` (default on) and
  `SINRICPRO_LOCAL_CONTROL_NO_MDNS` (UDP without the announcement).
- feat: `sinricpro_local_control_is_running()`.

### Fixes

- fix: an unreachable cloud no longer aborts `sinricpro_start()`. Only an invalid
  configuration is fatal; a connect failure logs, leaves the reconnect armed and
  keeps local control serving. Callers check `sinricpro_is_connected()`.
- fix: outgoing messages are signed over the exact bytes transmitted. The payload
  is serialised once and spliced into the envelope instead of being serialised a
  second time, and the signature is emitted last so a receiver can slice it out.
- fix: a message with no signature, or with a payload that could not be located,
  is no longer processed as if it had verified.
- fix: a request that fails verification now gets a signed "Signature is invalid"
  response instead of silence, so a client can tell a wrong app secret from an
  unreachable device.
- fix: signatures are compared in constant time.
- fix: payload extraction no longer treats a brace inside a JSON string as the end
  of the payload.
- fix: `sinricpro_core_send_event()` no longer leaks the caller's value object when
  it returns early because the SDK is stopped or the cloud is down.


| | |
|---|---|
| Transport | UDP port 3333, multicast group 224.9.9.9, unicast to the device too |
| Envelope | Identical to the cloud format, HMAC-SHA256 over the payload, base64 |
| Discovery | mDNS `_sinricpro._udp.local.`, host `sinricpro-<mac>` |
| TXT records | `deviceIds=<csv>`, `sdk=<version>`, `udp=1` |

Check it is up with `sinricpro_local_control_is_running()`. It is independent of
`sinricpro_is_connected()`: a device that has never reached SinricPro still
answers the LAN.

Verify from a desktop on the same network:

```bash
avahi-browse -r _sinricpro._udp     # Linux
dns-sd -B _sinricpro._udp           # macOS / Windows
```

### Notes

- The listener runs as its own FreeRTOS task (~6 KB stack by default). Device
  callbacks execute on it, so size it for your own callbacks.
- The mDNS responder costs roughly 40 KB of flash. The default 2 MB single-app
  partition layout has no room for it - the examples set
  `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y`.
- A request that fails signature verification gets a signed
  "Signature is invalid" reply rather than silence, so a client can tell a wrong
  app secret from an unreachable device.
- Android clients need a `WifiManager.MulticastLock` or mDNS returns nothing;
  iOS clients need the service type in `NSBonjourServices`.


## [1.2.1]

### Fixes

- fix: HMAC signature calculation now works on builds without `CONFIG_MBEDTLS_MD_C` (e.g. ESP-IDF 6.x size-optimized configs) and is ready for mbedTLS 4.x via the PSA Crypto API

## [1.1.2]

### Features

- fix: minor bugs

## [1.1.1]

### Features

- feat: examples for all device types added.

## [1.0.1]

### Features

- fix: Incorrect version 