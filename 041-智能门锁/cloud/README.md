# Local cloud MVP

This deployment provides a TLS MQTT endpoint for the lock and a small local
Node-RED web/API service. The web service binds to `127.0.0.1` intentionally;
publish it only through an authenticated VPN or reverse proxy.

## Start

Requirements: Docker Desktop and OpenSSL.

1. Copy `.env.example` to `.env` and replace all placeholders. Generate the
   device secret with a cryptographic random generator; the same 32 bytes must
   be provisioned into the device's protected configuration.
   Generate the Node-RED bcrypt password hash with the command shown in the
   example file and keep the quoted hash intact.
2. From PowerShell, run:

   ```powershell
   ./scripts/bootstrap.ps1 -BrokerPassword "a-long-unique-password"
   docker compose up -d
   ```

3. Open `http://127.0.0.1:1880/` and sign in. Node-RED's editor is at `/admin` and must not
   be exposed directly to a LAN or the Internet.
4. Configure ESP8266 MQTT TLS with the generated `mosquitto/certs/ca.crt`, user
   `door-001`, the chosen broker password, and port 8883.

The current ACL is deliberately single-device. Update the username, device ID,
and three topic paths together when naming the first lock. Use one account and
one command-signing secret per device when expanding beyond the MVP.
Set `SMART_LOCK_ALERT_WEBHOOK_URL` to an HTTPS webhook if tamper, door-ajar,
and duress events should be forwarded to a notification service.

## Backup and reset

Back up `.env`, `mosquitto/certs/ca.crt`, the device provisioning record, and
the Docker volumes. Never commit `.env`, private keys, or `mosquitto/passwords`.
The dashboard retains only the latest 100 event messages.
