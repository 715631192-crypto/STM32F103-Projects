# MVP architecture

## Trust boundary

The STM32 makes the final unlock decision. The cloud can request an action but
cannot directly drive the motor. Every remote command has a monotonically
increasing request ID, issue and expiry timestamps, an action, and an HMAC-SHA1
tag generated with the per-device secret.

```text
Phone browser -> local Node-RED HTTP (or HTTPS reverse proxy/VPN)
              -> Mosquitto MQTT/TLS -> ESP8266
                                                        -> UART -> STM32
Sensors -> STM32 decision -> motor / alarm / W25Q64 log -> MQTT event
```

Local fingerprint, RFID, owner PIN, visitor PIN, and TOTP continue to work when
the network is unavailable. Network loss must never leave the actuator in an
unlocked state.

## FreeRTOS ownership

| Task | Priority | Responsibility |
|---|---:|---|
| access | 4 | Sole owner of lock state and authentication decisions |
| input | 3 | Polls/receives keypad, fingerprint, RFID, door, tamper events |
| network | 2 | Receives signed commands and publishes queued notifications |
| log | 1 | Sole owner of W25Q64 erase/write operations |
| UI | 1 | Sole owner of OLED power and rendering; consumes UI messages |

Queues decouple slow Flash/network operations from the access-control task.
All queues and application tasks are statically allocated. The network task
places verified-format commands onto a queue; it never changes lock state.
Failed MQTT publications remain pending and retry with bounded exponential
backoff. Queue and storage failures are surfaced through the board diagnostic
hook instead of being silently discarded.
