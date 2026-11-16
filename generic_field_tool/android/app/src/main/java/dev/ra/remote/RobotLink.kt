// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.content.Context
import android.net.*
import dev.ra.remote.proto.DynamicProto
import dev.ra.remote.proto.Frames
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import java.net.NetworkInterface
import java.net.InetAddress
import java.net.Inet6Address
import java.io.Closeable

class LinkSwitched(message: String) : Exception(message)

/** Exactly one callback registration and Network object owner per robot session. */
class WifiSession(private val context: Context) : Closeable {
    private val manager = context.getSystemService(ConnectivityManager::class.java)
    private val current = MutableStateFlow<Network?>(null)
    private val callback = object : ConnectivityManager.NetworkCallback() {
        override fun onAvailable(network: Network) {
            current.value = network
        }

        override fun onLost(network: Network) {
            if (current.value == network) current.value = null
        }
    }

    init {
        manager.registerNetworkCallback(
            NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(),
            callback
        )
        current.value = manager.allNetworks.firstOrNull {
            manager.getNetworkCapabilities(it)
                ?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        }
    }

    suspend fun await(): Network = withTimeout(4000) { current.filterNotNull().first() }
    fun scoped(address: String, network: Network): String {
        if ('%' in address) return address
        val literal = InetAddress.getByName(address)
        require(literal is Inet6Address && literal.isLinkLocalAddress) { "ESP did not advertise an IPv6 link-local address" }
        val iface = manager.getLinkProperties(network)?.interfaceName
            ?: error("Wi-Fi interface unavailable")
        return Inet6Address.getByAddress(
            null,
            literal.address,
            NetworkInterface.getByName(iface)
        ).hostAddress ?: error("IPv6 scope unavailable")
    }

    override fun close() {
        runCatching { manager.unregisterNetworkCallback(callback) }
    }
}

/** BLE GATT, Wi-Fi callback and TLS socket live for the whole selected robot session.
 * BLE remains a bonded, connected standby while WLAN is active. No command is retried on failover.
 */
class RobotLink(
    private val context: Context,
    private val settings: ConnectionSettings,
    private val bootstrap: DynamicProto,
    private val progress: (String) -> Unit
) : Link {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val incoming = Channel<Result<Frames.Frame>>(64)
    private var wifi: WifiSession? = null
    private var ble: BleLink? = null
    private var tls: TlsLink? = null
    @Volatile
    private var selected: Link? = null
    @Volatile
    private var bleAvailable = false
    @Volatile
    private var closing = false
    override val label: String get() = selected?.label ?: "Connecting"
    override suspend fun open() {
        var host = settings.host.trim()
        var pin = settings.pin.replace(":", "").trim()
        if (settings.ble.isNotBlank()) {
            progress("BLE · native bonding · press EKF key 3 times within 3 seconds for first pairing")
            val b = BleLink(context, settings.ble.trim()); ble = b
            b.open(); bleAvailable = true
            progress("Reading Wi-Fi endpoint over bonded BLE")
            b.requestInfo(1)
            val info = withTimeout(8000) {
                var result: com.google.protobuf.DynamicMessage? = null
                while (result == null) {
                    val f = b.read(); if (f.kind() == 6) {
                        val parsed = bootstrap.parse(
                            "axi.telemetry.v1.LinkInfo",
                            f.payload()
                        ); if (DynamicProto.number(parsed, "request_id") == 1) result = parsed
                    }
                }
                result
            }
            val discovered = DynamicProto.field(info, "ipv6_linklocal") as String
            val fingerprint = DynamicProto.field(info, "certificate_sha256") as String
            require(
                DynamicProto.number(
                    info,
                    "port"
                ) == 7443 && fingerprint.matches(Regex("[0-9a-f]{64}"))
            ) { "Invalid BLE transport metadata" }
            // BLE is authenticated with the OS-held native bond. Never put endpoint data in business messages.
            if (discovered.isNotBlank()) host = discovered
            if (pin.isNotBlank()) require(
                pin.equals(
                    fingerprint,
                    true
                )
            ) { "Stored certificate pin differs from bonded robot" }
            pin = fingerprint
        }
        if (settings.mode != "BLE" && host.isNotBlank() && pin.isNotBlank()) {
            try {
                progress("Opening Wi-Fi link-local on this session's Wi-Fi network")
                val monitor = WifiSession(context); wifi = monitor
                val network = monitor.await()
                val endpoint = monitor.scoped(host, network)
                val t = TlsLink(endpoint, pin, network); tls = t; t.open(); selected = t
            } catch (e: Exception) {
                tls?.close(); tls =
                    null; if (settings.mode == "WLAN") throw e; progress("Wi-Fi unavailable · using connected BLE")
            }
        }
        if (selected == null) {
            require(settings.mode != "WLAN") { "No WLAN endpoint discovered" }; selected =
                ble ?: error("Enter a robot BLE address or WLAN endpoint")
        }
        ble?.let { b ->
            scope.launch {
                try {
                    while (isActive) {
                        val f = b.read(); if (selected === b) incoming.send(Result.success(f))
                    }
                } catch (e: Exception) {
                    bleAvailable =
                        false; if (!closing && selected === b) incoming.send(Result.failure(e))
                }
            }
        }
        tls?.let { t ->
            scope.launch {
                try {
                    while (isActive) {
                        val f = t.read(); if (selected === t) incoming.send(Result.success(f))
                    }
                } catch (e: Exception) {
                    if (!closing) {
                        val b = ble
                        if (settings.mode == "Auto" && b != null && bleAvailable) {
                            selected =
                                b; t.close(); incoming.send(Result.failure(LinkSwitched("WLAN lost; switched to connected BLE. Pending command unconfirmed; no replay.")))
                        } else incoming.send(Result.failure(e))
                    }
                }
            }
        }
    }

    override suspend fun read(): Frames.Frame = incoming.receive().getOrThrow()
    override suspend fun send(kind: Int, bytes: ByteArray) {
        (selected ?: error("Disconnected")).send(kind, bytes)
    }

    suspend fun provision(ssid: String, password: String) {
        (ble ?: error("BLE connection required")).provision(ssid, password)
    }

    override fun close() {
        closing =
            true; tls?.close(); ble?.close(); wifi?.close(); scope.cancel(); incoming.close(); selected =
            null
    }
}
