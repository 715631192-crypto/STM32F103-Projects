# MQTT protocol v1

All topics use QoS 1 and UTF-8 JSON. Replace `<device-id>` with the provisioned
identifier.

| Direction | Topic | Retained |
|---|---|---|
| Device to broker | `smartlock/<device-id>/state` | yes |
| Device to broker | `smartlock/<device-id>/event` | no |
| Broker to device | `smartlock/<device-id>/command` | no |

The device publishes state after connecting and whenever lock/door/alarm state
changes. An event includes `sequence`, `timestamp`, `event_type`, `auth_method`,
`user_id`, and `result`. Do not publish a PIN, visitor code, TOTP secret,
fingerprint template, card UID, broker password, or device secret.

Event types are `1=boot`, `2=unlock`, `3=lock`, `4=authentication failure`,
`5=lockout`, `6=tamper`, `7=door ajar`, `8=network`, `9=duress`,
`10=storage failure`, and `11=queue overflow`. Tamper, door-ajar, and duress
events are eligible for the configured emergency webhook.

## Signed command

```json
{
  "request_id": 1720000000001,
  "issued_at": 1720000000,
  "expires_at": 1720000030,
  "action": 1,
  "visitor_code": "",
  "visitor_code_length": 0,
  "visitor_uses": 0,
  "authentication_tag": "40-lowercase-hex-characters"
}
```

Actions are `1=unlock`, `2=lock`, `3=clear lockout`, and `4=issue visitor`.
Visitor issue commands carry 6–12 decimal digits, an expiry no more than 60
minutes ahead, and 1–5 uses.

The authentication tag is HMAC-SHA1 over this exact 39-byte binary sequence:

| Offset | Size | Encoding |
|---:|---:|---|
| 0 | 8 | request ID, unsigned big-endian |
| 8 | 8 | issued Unix time, unsigned big-endian |
| 16 | 8 | expiry Unix time, unsigned big-endian |
| 24 | 1 | action |
| 25 | 12 | visitor ASCII digits followed by zero bytes |
| 37 | 1 | visitor code length |
| 38 | 1 | visitor uses |

The device verifies the HMAC in constant time, requires a strictly increasing
request ID, rejects expired or future-dated messages, then persists the last
accepted ID before acknowledging an effect. TLS protects MQTT transport; the
command HMAC provides end-to-end authenticity through the broker.
