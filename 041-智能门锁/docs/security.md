# Security decisions and limitations

## Implemented in the portable core

- RFC 6238 TOTP using HMAC-SHA1 with a configurable time window.
- Owner and visitor PINs represented as keyed HMAC tags, not plaintext storage.
- Virtual PIN matching with bounded input length.
- Three-strike lockout with an explicit expiry time.
- One shared keypad rate limit for owner PIN, TOTP, and visitor attempts.
- Signed remote commands with expiry and monotonically increasing replay state.
- One-time or limited-use visitor credentials.
- CRC-protected, monotonic event records for power-on recovery scanning.
- Tamper alarm as a latch that ordinary authentication cannot clear.
- Optional fingerprint-plus-PIN or RFID-plus-PIN authentication with timeout.
- Duress PIN/fingerprint events that unlock normally while emitting a distinct
  emergency event and turning off the display.
- Weekly time-window PINs with an explicit UTC offset.

## Required integration controls

- Unique device secrets provisioned outside source control.
- MQTT over TLS for the external listener; anonymous plaintext access only on
  the private Docker network used by Node-RED.
- RTC oscillator-stop detection. TOTP must be disabled when time is untrusted.
- The board layer must suppress TOTP events while DS3231 time is untrusted.
- Log queue overflow, storage failure, and network failure must produce visible
  diagnostic state and counters.
- Persisted security configuration must be versioned, CRC-protected, and
  committed with a two-copy or journaled power-loss-safe update.

## Prototype limitations

STM32F103C8T6 has no modern secure enclave or secure boot chain. Readout
protection raises the effort required to extract secrets but is not equivalent
to a certified secure element. RC522 card UIDs are cloneable, common optical
fingerprint modules make decisions outside the MCU trust boundary, and SG90 is
a hobby actuator. This MVP must not be marketed as certified financial-grade or
used as the sole protection for high-value property.
