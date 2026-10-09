package com.onenode.dropin.service

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Environment
import android.os.IBinder
import android.app.PendingIntent
import android.net.Uri
import android.webkit.MimeTypeMap
import androidx.core.content.FileProvider
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import java.io.DataInputStream
import java.io.File
import java.net.ServerSocket

class ReceiverService : Service() {

    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.IO + job)
    private var serverSocket: ServerSocket? = null
    private var pingSocket: ServerSocket? = null
    private val CHANNEL_ID = "one_node_channel"
    private val PORT = 45679
    private val PING_PORT = 45681

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        startForeground(1, buildNotification("One Node — ready to receive files"))
        startListening()
        startPingServer()
    }

    private fun startListening() {
        scope.launch {
            try {
                serverSocket = ServerSocket(PORT)
                while (true) {
                    val client = serverSocket!!.accept()
                    launch { handleIncomingFile(client) }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun startPingServer() {
        scope.launch {
            try {
                pingSocket = ServerSocket(PING_PORT)
                while (true) {
                    val client = pingSocket!!.accept()
                    launch {
                        try {
                            val reader = client.getInputStream().bufferedReader()
                            val writer = client.getOutputStream().bufferedWriter()
                            val msg = reader.readLine()
                            if (msg == "ping") {
                                writer.write("pong")
                                writer.newLine()
                                writer.flush()
                            }
                        } catch (e: Exception) {
                            e.printStackTrace()
                        } finally {
                            client.close()
                        }
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun handleIncomingFile(socket: java.net.Socket) {
        try {
            socket.soTimeout = 5000 // Handshake timeout
            val input = DataInputStream(socket.getInputStream())
            val output = java.io.DataOutputStream(socket.getOutputStream())

            val prefs = getSharedPreferences("OneNodePrefs", Context.MODE_PRIVATE)
            val secretString = prefs.getString("pairing_secret", "")
            val secret = android.util.Base64.decode(secretString, android.util.Base64.DEFAULT)
            // myId is Android's ID, which is stored as device_id
            val myId = prefs.getString("device_id", "")!!.toByteArray()
            // peerId is Desktop's ID, which is stored as server_device_id
            val peerId = prefs.getString("server_device_id", "")!!.toByteArray()

            val serverNonce = ByteArray(32)
            java.security.SecureRandom().nextBytes(serverNonce)
            output.writeByte(1)
            output.write(serverNonce)
            output.flush()

            val clientNonce = ByteArray(32)
            val hmacC = ByteArray(32)
            input.readFully(clientNonce)
            input.readFully(hmacC)

            val idLen = input.readUnsignedShort()
            if (idLen > 1024) { socket.close(); return }
            val recvId = ByteArray(idLen)
            input.readFully(recvId)

            if (!recvId.contentEquals(peerId)) {
                socket.close()
                return
            }

            val macCInst = javax.crypto.Mac.getInstance("HmacSHA256")
            macCInst.init(javax.crypto.spec.SecretKeySpec(secret, "HmacSHA256"))
            macCInst.update("ONv1-C".toByteArray())
            macCInst.update(serverNonce)
            macCInst.update(recvId)
            macCInst.update(0.toByte())
            macCInst.update(myId)
            val expectedHmacC = macCInst.doFinal()

            if (!java.security.MessageDigest.isEqual(hmacC, expectedHmacC)) {
                socket.close()
                return
            }

            val macSInst = javax.crypto.Mac.getInstance("HmacSHA256")
            macSInst.init(javax.crypto.spec.SecretKeySpec(secret, "HmacSHA256"))
            macSInst.update("ONv1-S".toByteArray())
            macSInst.update(clientNonce)
            macSInst.update(myId)
            macSInst.update(0.toByte())
            macSInst.update(recvId)
            val hmacS = macSInst.doFinal()
            output.write(hmacS)
            output.flush()

            socket.soTimeout = 30000 // Revert timeout for file transfer

            // Read filename length (4 bytes)
            val nameLen = input.readInt()

            // Read filename
            val nameBytes = ByteArray(nameLen)
            input.readFully(nameBytes)
            val fileName = String(nameBytes)

            // Read file size (8 bytes)
            val fileSize = input.readLong()

            // Save to Downloads/OneNode/
            val dir = File(
                Environment.getExternalStoragePublicDirectory(
                    Environment.DIRECTORY_DOWNLOADS), "OneNode")
            dir.mkdirs()

            val outFile = File(dir, fileName)
            val fileOutput = outFile.outputStream()
            val buffer = ByteArray(8192)
            var received = 0L
            var lastPercent = 0

            while (received < fileSize) {
                val toRead = minOf(buffer.size.toLong(), fileSize - received).toInt()
                val read = input.read(buffer, 0, toRead)
                if (read == -1) break
                fileOutput.write(buffer, 0, read)
                received += read
                
                val currentPercent = ((received * 100) / fileSize).toInt()
                if (currentPercent > lastPercent) {
                    lastPercent = currentPercent
                    updateProgressNotification(fileName, currentPercent)
                }
            }

            fileOutput.flush()
            fileOutput.close()
            socket.close()

            showCompletionNotification(fileName, outFile)

        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun updateProgressNotification(fileName: String, percent: Int) {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setContentTitle("Receiving $fileName")
            .setProgress(100, percent, false)
            .setOngoing(true)
            .build()
        nm.notify(2, notification)
    }

    private fun showCompletionNotification(fileName: String, file: File) {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.cancel(2) // cancel progress
        
        val uri = FileProvider.getUriForFile(this, "$packageName.fileprovider", file)
        val ext = file.extension
        val mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext) ?: "*/*"
        
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, mimeType)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        
        val pendingIntent = PendingIntent.getActivity(this, 0, intent, 
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)

        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_download_done)
            .setContentTitle("One Node")
            .setContentText("File received: $fileName")
            .setAutoCancel(true)
            .setContentIntent(pendingIntent)
            .build()
        nm.notify(System.currentTimeMillis().toInt(), notification)
    }

    private fun buildNotification(text: String) =
        NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_download_done)
            .setContentTitle("One Node")
            .setContentText(text)
            .setOngoing(true)
            .build()

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID, "One Node", NotificationManager.IMPORTANCE_DEFAULT)
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.createNotificationChannel(channel)
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
        serverSocket?.close()
        pingSocket?.close()
    }
}
