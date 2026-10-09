# Security and Limitations

One Node is designed to provide seamless file sharing across your personal devices on a local network. However, there are some important security considerations you should be aware of:

## Current Security Model

1. **Local Network Only**: One Node operates entirely on your local area network (LAN). It does not route traffic through the internet or third-party servers.
2. **Initial Pairing (Vulnerable Window)**: During the initial 60-second pairing window, a 6-digit PIN is used to authorize the pairing, and a 32-byte secure random secret is exchanged in plaintext. A passive attacker sniffing your network traffic during this exact 60-second window could capture the secret and later impersonate the phone. 
3. **Mutual Authentication (HMAC)**: After pairing, all subsequent connections use a cryptographic challenge-response (HMAC-SHA256) based on the shared secret. Devices verify each other's identity before any data is sent. An attacker without the secret cannot impersonate a paired device.
4. **Unencrypted File Transfers**: File data is currently transmitted over the local network without encryption. While the endpoints are authenticated, the file contents themselves can be intercepted by anyone monitoring your local network traffic. 

## Roadmap

- **TLS Encryption**: We plan to implement TLS to encrypt both the initial pairing (via an ECDH key exchange) and the file transfers, which will fully protect the pairing secret and file contents from network sniffers.
- **Multi-Device Support**: The UI displays paired devices as a list, but currently, only one active pairing is fully supported at a time. Multi-device support will be fully implemented in a future update.

## Best Practices
- Pair your devices on a trusted network (like your home Wi-Fi), rather than a public or open network (like a coffee shop).
- If you suspect a device pairing has been compromised, simply click "Unpair" on either device. This completely removes the shared secret and severs the link.
