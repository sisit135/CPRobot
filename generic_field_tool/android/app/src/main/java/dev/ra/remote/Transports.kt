// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.*
import android.os.Build
import dev.ra.remote.proto.Frames
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.io.Closeable
import java.net.InetSocketAddress
import java.net.Socket
import java.security.MessageDigest
import java.security.SecureRandom
import java.security.cert.X509Certificate
import java.util.UUID
import javax.net.ssl.*

interface Link : Closeable {
    val label: String
    suspend fun open()
    suspend fun read(): Frames.Frame
    suspend fun send(kind: Int, bytes: ByteArray)
}

class TlsLink(
    private val host: String,
    private val pin: String,
    private val network: android.net.Network? = null
) : Link {
    override val label = "WLAN · TLS"

    @Volatile
    private var socket: Socket? = null
    override suspend fun open() = withContext(Dispatchers.IO) {
        require(pin.matches(Regex("[0-9a-fA-F]{64}"))) { "Set the ESP certificate SHA-256 fingerprint first" }
        val trust = object : X509TrustManager {
            override fun getAcceptedIssuers() = emptyArray<X509Certificate>()
            override fun checkClientTrusted(c: Array<out X509Certificate>?, a: String?) = Unit
            override fun checkServerTrusted(c: Array<out X509Certificate>?, a: String?) {
                require(!c.isNullOrEmpty()) { "Missing server certificate" }
                c[0].checkValidity()
                val expected = pin.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
                require(
                    MessageDigest.isEqual(
                        MessageDigest.getInstance("SHA-256").digest(c[0].encoded), expected
                    )
                ) { "Certificate fingerprint mismatch" }
            }
        }
        val context =
            SSLContext.getInstance("TLS").apply { init(null, arrayOf(trust), SecureRandom()) }
        val raw = network?.socketFactory?.createSocket() ?: Socket()
        socket = raw
        raw.connect(InetSocketAddress(host, 7443), 4500)
        val s = context.socketFactory.createSocket(raw, host, 7443, true) as SSLSocket
        socket = s
        try {
            s.soTimeout = 5000; s.startHandshake(); s.soTimeout = 0
        } catch (e: Exception) {
            s.close(); throw e
        }
    }

    override suspend fun read() = withContext(Dispatchers.IO) {
        Frames.read(
            (socket ?: error("Disconnected")).getInputStream()
        )
    }

    override suspend fun send(kind: Int, bytes: ByteArray) = withContext(Dispatchers.IO) {
        val s = socket ?: error("Disconnected"); s.getOutputStream()
        .write(Frames.encode(kind, bytes)); s.getOutputStream().flush()
    }

    override fun close() {
        runCatching { socket?.close() }; socket = null
    }
}

@SuppressLint("MissingPermission")
class BleLink(private val context: Context, private val address: String) : Link {
    override val label = "BLE · Nordic UART"
    private val packets = Channel<Frames.Frame>(64)
    private val chunks = Frames.Chunks()
    private val connected = CompletableDeferred<Unit>()
    private val discovered = CompletableDeferred<Unit>()
    private val negotiated = CompletableDeferred<Int>()
    private val subscribed = CompletableDeferred<Unit>()
    private val writes = Channel<Int>(1)
    private val writeMutex = Mutex()

    @Volatile
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var mtu = 23
    private val service = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    private val receive = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    private val transmit = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    private val cccd = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private fun gattError(status: Int) =
        IllegalStateException("Bluetooth operation failed ($status)")

    private fun accept(value: ByteArray) {
        runCatching { chunks.accept(value) }.onSuccess {
            if (it != null && packets.trySend(it).isFailure) packets.close(
                IllegalStateException("BLE receive queue overflow")
            )
        }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) connected.complete(
                Unit
            )
            else {
                val e =
                    gattError(status); connected.completeExceptionally(e); discovered.completeExceptionally(
                    e
                ); negotiated.completeExceptionally(e); subscribed.completeExceptionally(e); packets.close(
                    e
                ); writes.close(e)
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status == 0) discovered.complete(Unit) else discovered.completeExceptionally(
                gattError(status)
            )
        }

        override fun onMtuChanged(g: BluetoothGatt, value: Int, status: Int) {
            if (status == 0) negotiated.complete(value) else negotiated.completeExceptionally(
                gattError(status)
            )
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            if (status == 0) subscribed.complete(Unit) else subscribed.completeExceptionally(
                gattError(status)
            )
        }

        override fun onCharacteristicWrite(
            g: BluetoothGatt,
            c: BluetoothGattCharacteristic,
            status: Int
        ) {
            writes.trySend(status)
        }

        @Deprecated("Legacy Android callback")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < 33) accept(c.value ?: byteArrayOf())
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            c: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            accept(value)
        }
    }

    private suspend fun bond(device: BluetoothDevice) {
        if (device.bondState == BluetoothDevice.BOND_BONDED) return
        val done = CompletableDeferred<Unit>()
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                @Suppress("DEPRECATION") val d =
                    i.getParcelableExtra<BluetoothDevice>(BluetoothDevice.EXTRA_DEVICE)
                if (d?.address != device.address) return
                when (i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, 0)) {
                    BluetoothDevice.BOND_BONDED -> done.complete(Unit); BluetoothDevice.BOND_NONE -> done.completeExceptionally(
                    IllegalStateException("Pairing cancelled")
                )
                }
            }
        }
        if (Build.VERSION.SDK_INT >= 33) context.registerReceiver(
            receiver,
            IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED),
            Context.RECEIVER_EXPORTED
        )
        else @Suppress("DEPRECATION") context.registerReceiver(
            receiver,
            IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
        )
        try {
            check(device.createBond()) { "Could not start pairing" }; withTimeout(45000) { done.await() }
        } finally {
            context.unregisterReceiver(receiver)
        }
    }

    override suspend fun open() {
        val manager = context.getSystemService(BluetoothManager::class.java)
        val adapter = manager?.adapter ?: error("Bluetooth unavailable")
        check(adapter.isEnabled) { "Enable Bluetooth first" }
        val device = adapter.getRemoteDevice(address)
        bond(device)
        withTimeout(15000) {
            val g = withContext(Dispatchers.Main) {
                device.connectGatt(
                    context,
                    false,
                    callback,
                    BluetoothDevice.TRANSPORT_LE
                )
            }
            gatt = g; connected.await(); check(g.discoverServices()); discovered.await()
            check(g.requestMtu(247)); mtu =
            negotiated.await(); require(mtu >= 64) { "Remote config needs ATT MTU 64 or larger" }
            val svc = g.getService(service) ?: error("Nordic UART service unavailable")
            rx = svc.getCharacteristic(receive) ?: error("NUS RX missing")
            val tx = svc.getCharacteristic(transmit) ?: error("NUS TX missing")
            check(g.setCharacteristicNotification(tx, true))
            val d = tx.getDescriptor(cccd) ?: error("NUS notification descriptor missing")
            if (Build.VERSION.SDK_INT >= 33) check(
                g.writeDescriptor(
                    d,
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                ) == BluetoothStatusCodes.SUCCESS
            )
            else {
                @Suppress("DEPRECATION") d.value =
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE; @Suppress("DEPRECATION") check(
                    g.writeDescriptor(d)
                )
            }
            subscribed.await()
        }
    }

    override suspend fun read(): Frames.Frame = packets.receive()
    private suspend fun raw(bytes: ByteArray) = writeMutex.withLock {
        require(bytes.size <= mtu - 3) { "GATT write exceeds negotiated MTU" }
        val g = gatt ?: error("Disconnected")
        val c = rx ?: error("NUS RX unavailable")
        if (Build.VERSION.SDK_INT >= 33) check(
            g.writeCharacteristic(
                c,
                bytes,
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            ) == BluetoothStatusCodes.SUCCESS
        )
        else {
            @Suppress("DEPRECATION") c.writeType =
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT; @Suppress("DEPRECATION") c.value =
                bytes; @Suppress("DEPRECATION") check(g.writeCharacteristic(c))
        }
        check(withTimeout(5000) { writes.receive() } == 0) { "GATT write failed" }
    }

    override suspend fun send(kind: Int, bytes: ByteArray) {
        require(kind == 2 || kind == 4 || kind == 8 || kind == 9); raw(
            byteArrayOf(kind.toByte()) + Frames.encode(
                kind,
                bytes
            )
        )
    }

    suspend fun requestInfo(id: Int) {
        raw(
            byteArrayOf(
                0xf1.toByte(),
                id.toByte(),
                (id shr 8).toByte(),
                (id shr 16).toByte(),
                (id shr 24).toByte()
            )
        )
    }

    suspend fun provision(ssid: String, password: String) {
        val a = ssid.toByteArray(Charsets.UTF_8)
        val b =
            password.toByteArray(Charsets.UTF_8); require(a.size in 1..32 && (b.isEmpty() || b.size in 8..63)); raw(
            byteArrayOf(0xf0.toByte(), a.size.toByte()) + a + byteArrayOf(b.size.toByte()) + b
        )
    }

    override fun close() {
        runCatching { gatt?.disconnect(); gatt?.close() }; gatt =
            null; packets.close(); writes.close(); chunks.reset()
    }
}
