package com.onenode.dropin

import org.junit.Assert.assertArrayEquals
import org.junit.Test
import java.security.MessageDigest
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class HmacTest {
    
    private fun hexStringToByteArray(s: String): ByteArray {
        val len = s.length
        val data = ByteArray(len / 2)
        var i = 0
        while (i < len) {
            data[i / 2] = ((Character.digit(s[i], 16) shl 4) + Character.digit(s[i + 1], 16)).toByte()
            i += 2
        }
        return data
    }

    @Test
    fun testHmacVectors() {
        val secret = ByteArray(32) { 1.toByte() }
        val serverNonce = ByteArray(32) { 2.toByte() }
        val clientNonce = ByteArray(32) { 3.toByte() }
        val serverId = "ServerID".toByteArray()
        val clientId = "ClientID".toByteArray()

        val expectedHmacCHex = "aecb443a2e94a4c9b9e784f4f8221f5656aca36f3ceaa18df7c4a7a399db4b80"
        val expectedHmacSHex = "4563e5b38978731bf5103043c6a9191175c355a1a3842a5ee4cff74f42d0ce8c"

        val macC = Mac.getInstance("HmacSHA256")
        macC.init(SecretKeySpec(secret, "HmacSHA256"))
        macC.update("ONv1-C".toByteArray())
        macC.update(serverNonce)
        macC.update(clientId)
        macC.update(0.toByte())
        macC.update(serverId)
        val hmacC = macC.doFinal()

        val macS = Mac.getInstance("HmacSHA256")
        macS.init(SecretKeySpec(secret, "HmacSHA256"))
        macS.update("ONv1-S".toByteArray())
        macS.update(clientNonce)
        macS.update(serverId)
        macS.update(0.toByte())
        macS.update(clientId)
        val hmacS = macS.doFinal()

        assertArrayEquals(hexStringToByteArray(expectedHmacCHex), hmacC)
        assertArrayEquals(hexStringToByteArray(expectedHmacSHex), hmacS)
    }
}
