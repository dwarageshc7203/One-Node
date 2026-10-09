# One Node Protocol

## Handshake (Version 1)

All strings are UTF-8 encoded.

### 1. Pairing (TCP 45678)
1. **Client** (e.g. Android) connects to Server (Desktop) and sends a JSON payload separated by a newline:
```json
{
  "code": "123456",
  "device": "Pixel",
  "device_id": "c3f829d1-...", 
  "version": 1
}
```
2. **Server** validates the code. If correct, generates a 32-byte secure random secret and sends:
```json
{
  "status": "ok",
  "device_id": "8b9e12a4-...",
  "device_name": "Desktop",
  "secret": "base64-encoded-32-byte-secret"
}
```
If incorrect, server replies `{"status": "error", "message": "..."}`.

### 2. File Transfer Connection (TCP 45679 for Sending, 45680 for Receiving)
Authentication must happen before sending/receiving file streams. 

1. **Client** connects.
2. **Server** sends: `[1 byte version = 0x01][32 bytes server_nonce]`
3. **Client** replies: `[32 bytes client_nonce][32 bytes hmac_c][2 bytes own_device_id_len (BigEndian)][own_device_id bytes]`
   where `hmac_c = HMAC-SHA256(secret, "ONv1-C" || server_nonce || own_device_id || 0x00 || peer_device_id)`
4. **Server** looks up the `secret` matching the client's `own_device_id` (currently, only one pairing is supported so it validates against the single stored `peer_device_id`). The server verifies `hmac_c` in constant time. If invalid, the connection is closed. If valid, sends:
   `[32 bytes hmac_s]`
   where `hmac_s = HMAC-SHA256(secret, "ONv1-S" || client_nonce || own_device_id || 0x00 || peer_device_id)`
5. **Client** verifies `hmac_s` in constant time. If invalid, the connection is closed.

After authentication, the connection is open for file headers and payloads.

## Key Storage Nomenclature
To avoid confusion during implementation, applications should store IDs in settings as:
- `own_device_id` (this device's UUID)
- `peer_device_id` (the paired device's UUID)

## Security Weakness
During the first pairing, the 32-byte secret is sent in plaintext over the LAN. A passive sniffer on the same network could capture it during the 60-second pairing window and later impersonate the phone. TLS or an ECDH key exchange is planned for the future. File data is unencrypted.
