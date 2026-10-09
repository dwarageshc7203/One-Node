import hmac
import hashlib
import os

serverNonce = os.urandom(32)
clientNonce = os.urandom(32)
secret = os.urandom(32)
desktop_id = b"desktop-uuid-123"
android_id = b"android-uuid-456"

# Android computes hmacC:
macC = hmac.new(secret, digestmod=hashlib.sha256)
macC.update(b"ONv1-C")
macC.update(serverNonce)
macC.update(desktop_id)
macC.update(b"\x00")
macC.update(android_id)
expectedHmacC = macC.digest()

# Desktop computes hmacC:
macC2 = hmac.new(secret, digestmod=hashlib.sha256)
msgC = b"ONv1-C" + serverNonce + desktop_id + b"\x00" + android_id
macC2.update(msgC)
hmacC = macC2.digest()

print("hmacC match:", hmacC == expectedHmacC)

# Android computes hmacS:
macS = hmac.new(secret, digestmod=hashlib.sha256)
macS.update(b"ONv1-S")
macS.update(clientNonce)
macS.update(android_id)
macS.update(b"\x00")
macS.update(desktop_id)
hmacS = macS.digest()

# Desktop expects msgS:
macS2 = hmac.new(secret, digestmod=hashlib.sha256)
msgS = b"ONv1-S" + clientNonce + android_id + b"\x00" + desktop_id
macS2.update(msgS)
expectedHmacS = macS2.digest()

print("hmacS match:", hmacS == expectedHmacS)
