// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.Manifest
import android.annotation.SuppressLint
import android.app.*
import android.bluetooth.*
import android.content.*
import android.content.pm.PackageManager
import android.os.*
import androidx.annotation.RequiresApi
import androidx.core.content.ContextCompat
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

fun loadVideoTracingSettings(context: Context): VideoTracingSettings {
    val prefs = context.getSharedPreferences("connection", 0)
    return VideoTracingSettings(
        prefs.getBoolean("video.enabled", false),
        prefs.getString("video.pattern", "").orEmpty(),
        runCatching {
            VideoTracePatternMode.valueOf(
                prefs.getString(
                    "video.mode",
                    "GLOB"
                )!!
            )
        }.getOrDefault(VideoTracePatternMode.GLOB)
    )
}

/** Keeps the HID registration foreground while the operator pairs in system settings. */
@RequiresApi(28)
@SuppressLint("MissingPermission")
class VideoTracingService : Service() {
    class LocalBinder(val service: VideoTracingService) : Binder()

    private val binder = LocalBinder(this)
    private val handler = Handler(Looper.getMainLooper())
    private val mutableStatus =
        MutableStateFlow(VideoTracingStatus(message = "Starting Bluetooth shutter"))
    val status = mutableStatus.asStateFlow()
    private val mutableEvents = MutableSharedFlow<String>(extraBufferCapacity = 32)
    val events = mutableEvents.asSharedFlow()
    private var adapter: BluetoothAdapter? = null
    private var hid: BluetoothHidDevice? = null
    private var host: BluetoothDevice? = null
    private var pressedHost: BluetoothDevice? = null
    private var registered = false
    private var registering = false
    private var opening = false
    private var destroyed = false
    private var settings: VideoTracingSettings? = null
    private var trigger: VideoTraceTrigger? = null
    private val prefs by lazy { getSharedPreferences("connection", 0) }
    private val release = Runnable { releaseKey() }

    private val callback = object : BluetoothHidDevice.Callback() {
        override fun onAppStatusChanged(pluggedDevice: BluetoothDevice?, isRegistered: Boolean) {
            if (destroyed) return
            registering = false; registered = isRegistered
            if (isRegistered) {
                update("Shutter ready; pair or connect the recording phone in Bluetooth settings")
                refreshHosts()
                // Android supplies the previously plugged host; do not try every bonded device.
                if (pluggedDevice != null && hasPermission()) runCatching {
                    hid?.connect(
                        pluggedDevice
                    )
                }
            } else {
                host = null; pressedHost = null
                update("Bluetooth HID registration unavailable; reopen Video tracing to retry")
            }
        }

        override fun onConnectionStateChanged(device: BluetoothDevice, state: Int) {
            if (destroyed) return
            when (state) {
                BluetoothProfile.STATE_CONNECTED -> {
                    host = device; update("Connected to ${deviceName(device)}")
                }

                BluetoothProfile.STATE_DISCONNECTED -> if (host?.address == device.address) {
                    host = null; pressedHost =
                        null; update("Recording phone disconnected; waiting for reconnection")
                }
            }
            refreshHosts()
        }

        override fun onGetReport(device: BluetoothDevice, type: Byte, id: Byte, bufferSize: Int) {
            if (!hasPermission()) return
            runCatching {
                if (type == BluetoothHidDevice.REPORT_TYPE_INPUT && id.toInt() == REPORT_ID)
                    hid?.replyReport(
                        device,
                        type,
                        id,
                        byteArrayOf(if (pressedHost?.address == device.address) 1 else 0)
                    )
                else hid?.reportError(device, BluetoothHidDevice.ERROR_RSP_UNSUPPORTED_REQ)
            }
        }
    }
    private val profile = object : BluetoothProfile.ServiceListener {
        override fun onServiceConnected(kind: Int, proxy: BluetoothProfile) {
            opening = false
            if (destroyed) {
                adapter?.closeProfileProxy(kind, proxy); return
            }
            hid = proxy as? BluetoothHidDevice
            registerHid()
        }

        override fun onServiceDisconnected(kind: Int) {
            hid = null; registered = false; registering = false; opening = false; host =
                null; pressedHost = null
            if (!destroyed) update("Bluetooth HID service disconnected")
        }
    }
    private val bluetoothState = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (intent.action == BluetoothAdapter.ACTION_STATE_CHANGED) {
                if (intent.getIntExtra(
                        BluetoothAdapter.EXTRA_STATE,
                        -1
                    ) == BluetoothAdapter.STATE_ON
                ) startHid()
                else if (intent.getIntExtra(
                        BluetoothAdapter.EXTRA_STATE,
                        -1
                    ) == BluetoothAdapter.STATE_OFF
                ) {
                    registered = false; registering = false; host = null; pressedHost =
                        null; update("Turn on Bluetooth in system settings")
                }
            } else if (intent.action == BluetoothDevice.ACTION_BOND_STATE_CHANGED) refreshHosts()
        }
    }

    override fun onCreate() {
        super.onCreate()
        val notifications = getSystemService(NotificationManager::class.java)
        notifications.createNotificationChannel(
            NotificationChannel(
                CHANNEL,
                "Video tracing",
                NotificationManager.IMPORTANCE_LOW
            )
        )
        val open = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val notification =
            Notification.Builder(this, CHANNEL).setSmallIcon(android.R.drawable.ic_menu_camera)
                .setContentTitle("Video tracing shutter")
                .setContentText("Bluetooth shutter active · open the app to configure or disable")
                .setContentIntent(open).setOngoing(true).build()
        startForeground(2401, notification)
        adapter = getSystemService(BluetoothManager::class.java)?.adapter
        ContextCompat.registerReceiver(this, bluetoothState, IntentFilter().apply {
            addAction(BluetoothAdapter.ACTION_STATE_CHANGED); addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
        }, ContextCompat.RECEIVER_EXPORTED)
    }

    override fun onBind(intent: Intent) = binder
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val next = loadVideoTracingSettings(this)
        if (!next.enabled) {
            stopSelf(); return START_NOT_STICKY
        }
        if (next != settings) {
            val candidate = runCatching {
                VideoTraceTrigger(
                    next,
                    prefs.getBoolean("video.armed", false)
                )
            }.getOrElse {
                update("Invalid video tracing pattern"); stopSelf(); return START_NOT_STICKY
            }
            settings = next; trigger = candidate
            mutableStatus.update {
                it.copy(
                    lastTrigger = prefs.getString(
                        "video.lastTrigger",
                        null
                    )
                )
            }
            update(if (candidate.armed) "Armed; starting Bluetooth shutter" else "Disarmed; connect the recording phone and arm when ready")
        }
        startHid()
        return START_NOT_STICKY
    }

    private fun hasPermission() =
        Build.VERSION.SDK_INT < 31 || checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED

    private fun update(message: String) {
        mutableStatus.update {
            it.copy(
                message = message,
                connected = host != null,
                armed = trigger?.armed == true
            )
        }
    }

    private fun deviceName(device: BluetoothDevice) =
        runCatching { device.name ?: device.address }.getOrDefault("recording phone")

    private fun startHid() {
        if (!hasPermission()) {
            update("Bluetooth permission required"); return
        }
        val bluetooth =
            adapter ?: run { update("Bluetooth HID is unavailable on this device"); return }
        if (!bluetooth.isEnabled) {
            update("Turn on Bluetooth in system settings"); return
        }
        refreshHosts()
        if (hid != null) {
            registerHid(); return
        }
        if (opening) return
        opening = true
        val accepted = runCatching {
            bluetooth.getProfileProxy(
                this,
                profile,
                BluetoothProfile.HID_DEVICE
            )
        }.getOrDefault(false)
        if (!accepted) {
            opening = false; update("This device does not provide the Bluetooth HID Device profile")
        }
    }

    private fun registerHid() {
        if (registered || registering || !hasPermission()) return
        registering = true
        val accepted = runCatching {
            hid?.registerApp(
                BluetoothHidDeviceAppSdpSettings(
                    "Field Tool Shutter", "Video tracing remote shutter", "CPRobot",
                    BluetoothHidDevice.SUBCLASS1_NONE, DESCRIPTOR
                ), null, null, mainExecutor, callback
            ) == true
        }.getOrDefault(false)
        if (!accepted) {
            registering =
                false; update("Unable to register Bluetooth shutter; another HID app may be active")
        }
    }

    private fun refreshHosts() {
        if (!hasPermission()) return
        val devices = runCatching {
            adapter?.bondedDevices.orEmpty().map { VideoTraceHost(it.address, deviceName(it)) }
                .sortedBy { it.name }
        }.getOrDefault(emptyList())
        mutableStatus.update { it.copy(hosts = devices) }
    }

    fun connectPhone(address: String) {
        if (!registered || !hasPermission()) {
            update("Wait for the Bluetooth shutter to be ready"); return
        }
        if (pressedHost != null) {
            update("Wait for the shutter key release before switching phones"); return
        }
        val device =
            runCatching { adapter?.bondedDevices?.find { it.address == address } }.getOrNull()
                ?: return
        if (host?.address == address) return
        if (host != null) {
            update("Disconnect the current recording phone in Bluetooth settings first"); return
        }
        if (runCatching { hid?.connect(device) == true }.getOrDefault(false)) update(
            "Connecting to ${
                deviceName(
                    device
                )
            }"
        )
        else update("Could not connect; select this device from the recording phone's Bluetooth settings")
    }

    fun arm() {
        if (pressedHost != null) {
            update("Wait for the shutter key release before arming"); return
        }
        if (!prefs.edit().putBoolean("video.armed", true).commit()) {
            update("Unable to save the armed state"); return
        }
        trigger?.arm(); update(if (host == null) "Armed; waiting for a recording phone" else "Armed for the next matching robot log")
    }

    /** Called on the main thread for new robot LogRecords only; never on log replay or UI rendering. */
    fun onRobotLog(line: LogLine) {
        val active = trigger ?: return
        if (!active.armed || host == null || !registered || !hasPermission()) return
        var attemptedAt: java.time.Instant? = null
        val sent = active.onLog(line.text) {
            // Persist consumption before any HID report: a process restart must not toggle the camera again.
            if (!prefs.edit().putBoolean("video.armed", false).commit()) return@onLog false
            val device = host ?: return@onLog false
            attemptedAt = java.time.Instant.now()
            val accepted = runCatching {
                hid?.sendReport(
                    device,
                    REPORT_ID,
                    byteArrayOf(1)
                ) == true
            }.getOrDefault(false)
            if (accepted) {
                pressedHost = device; handler.postDelayed(release, 100)
            }
            accepted
        }
        if (sent) {
            val marker =
                "$attemptedAt · log #${line.id} (${line.time} ${line.level}/${line.tag}; robot uptime ${line.robotTimeMs?.let { "${it} ms" } ?: "unknown"}) · HID shutter sent; camera state unknown"
            mutableStatus.update {
                it.copy(
                    armed = false,
                    lastTrigger = marker,
                    message = "Shutter sent; rearm before the next recording"
                )
            }
            prefs.edit().putString("video.lastTrigger", marker).apply()
            mutableEvents.tryEmit("$marker · ${line.text.take(256)}")
        } else if (!active.armed) {
            update("Matching log consumed the arm; shutter send unconfirmed. Check the camera before rearming")
            mutableEvents.tryEmit(
                "${attemptedAt ?: java.time.Instant.now()} · log #${line.id} (${line.time} ${line.level}/${line.tag}; robot uptime ${line.robotTimeMs?.let { "${it} ms" } ?: "unknown"}) · shutter send unconfirmed; disarmed; no automatic retry · ${
                    line.text.take(
                        256
                    )
                }"
            )
        }
    }

    private fun releaseKey() {
        val device = pressedHost ?: return
        if (hasPermission()) {
            val ok = runCatching {
                hid?.sendReport(
                    device,
                    REPORT_ID,
                    byteArrayOf(0)
                ) == true
            }.getOrDefault(false)
            if (!ok) {
                mutableEvents.tryEmit("Shutter key release unconfirmed; disconnect the recording phone before rearming")
                runCatching { hid?.disconnect(device) }
                host =
                    null; update("Key release unconfirmed; reconnect the recording phone before rearming")
            }
        }
        pressedHost = null
    }

    override fun onDestroy() {
        destroyed = true; handler.removeCallbacks(release); releaseKey()
        unregisterReceiver(bluetoothState)
        if (hasPermission()) runCatching { hid?.unregisterApp() }
        hid?.let { adapter?.closeProfileProxy(BluetoothProfile.HID_DEVICE, it) }
        hid = null; host = null; stopForeground(STOP_FOREGROUND_REMOVE)
        super.onDestroy()
    }

    companion object {
        private const val CHANNEL = "video_tracing"
        private const val REPORT_ID = 1

        // Consumer Control / Volume Increment: one input bit followed by seven padding bits.
        private val DESCRIPTOR = byteArrayOf(
            0x05,
            0x0c,
            0x09,
            0x01,
            0xa1.toByte(),
            0x01,
            0x85.toByte(),
            0x01,
            0x15,
            0x00,
            0x25,
            0x01,
            0x09,
            0xe9.toByte(),
            0x75,
            0x01,
            0x95.toByte(),
            0x01,
            0x81.toByte(),
            0x02,
            0x75,
            0x07,
            0x95.toByte(),
            0x01,
            0x81.toByte(),
            0x03,
            0xc0.toByte()
        )
    }
}
