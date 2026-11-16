// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.adl;

import android.annotation.SuppressLint;
import android.bluetooth.*;
import android.content.Context;
import android.os.Build;
import java.io.IOException;
import java.util.UUID;
import java.util.concurrent.*;

/** CyberPi's native BLE byte pipe. Uses 20-byte writes so ATT MTU 23 is sufficient. */
@SuppressLint("MissingPermission")
final class CyberPiBle {
    private final Context context;
    private final String address;
    private volatile BluetoothGatt gatt;
    private volatile IOException failure;
    private volatile boolean closed;
    private BluetoothGattCharacteristic write;
    private final CountDownLatch connected = new CountDownLatch(1);
    private final CountDownLatch discovered = new CountDownLatch(1);
    private final CountDownLatch subscribed = new CountDownLatch(1);
    private final BlockingQueue<Integer> writes = new ArrayBlockingQueue<>(2);
    private final BlockingQueue<byte[]> received = new ArrayBlockingQueue<>(128);
    private static UUID uuid(int shortId) { return UUID.fromString(String.format(java.util.Locale.ROOT, "0000%04x-0000-1000-8000-00805f9b34fb", shortId)); }

    CyberPiBle(Context context, String address) { this.context=context; this.address=address; }

    private void fail(String reason) {
        failure = new IOException(reason);
        connected.countDown(); discovered.countDown(); subscribed.countDown();
        writes.clear(); writes.offer(-1); received.clear(); received.offer(new byte[0]);
    }
    private void accept(byte[] bytes) {
        if (bytes.length==0) return;
        if (!received.offer(bytes.clone())) fail("CyberPi BLE receive overflow");
    }
    private void status(int code, CountDownLatch ready) {
        if (code != BluetoothGatt.GATT_SUCCESS) fail("CyberPi Bluetooth status " + code);
        ready.countDown();
    }
    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt g, int status, int state) {
            if (status==0 && state==BluetoothProfile.STATE_CONNECTED) connected.countDown();
            else fail("CyberPi Bluetooth disconnected ("+status+")");
        }
        @Override public void onServicesDiscovered(BluetoothGatt g, int status) { status(status,discovered); }
        @Override public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor d, int status) { status(status,subscribed); }
        @Override public void onCharacteristicWrite(BluetoothGatt g, BluetoothGattCharacteristic c, int status) { writes.offer(status); }
        @SuppressWarnings("deprecation")
        @Override public void onCharacteristicChanged(BluetoothGatt g, BluetoothGattCharacteristic c) {
            if (Build.VERSION.SDK_INT<33 && c.getValue()!=null) accept(c.getValue());
        }
        @Override public void onCharacteristicChanged(BluetoothGatt g, BluetoothGattCharacteristic c, byte[] value) { accept(value); }
    };

    private void check() throws IOException {
        if (failure!=null) throw failure;
        if (closed) throw new IOException("CyberPi transport closed");
    }
    private void await(CountDownLatch ready) throws Exception {
        if (!ready.await(15,TimeUnit.SECONDS)) throw new IOException("CyberPi Bluetooth operation timed out");
        check();
    }
    @SuppressWarnings("deprecation")
    void open() throws Exception {
        check();
        BluetoothAdapter adapter=context.getSystemService(BluetoothManager.class).getAdapter();
        if(adapter==null || !adapter.isEnabled()) throw new IOException("Enable Bluetooth first");
        BluetoothGatt opened=adapter.getRemoteDevice(address).connectGatt(context,false,callback,BluetoothDevice.TRANSPORT_LE);
        gatt=opened;
        if(closed) { opened.close(); check(); }
        await(connected);
        if(!opened.discoverServices()) throw new IOException("Cannot discover CyberPi services");
        await(discovered);
        BluetoothGattService service=opened.getService(uuid(0xffe1));
        if(service==null) throw new IOException("CyberPi BLE service FFE1 missing");
        write=service.getCharacteristic(uuid(0xffe3));
        BluetoothGattCharacteristic notify=service.getCharacteristic(uuid(0xffe2));
        if(write==null || notify==null) throw new IOException("CyberPi BLE characteristics missing");
        if(!opened.setCharacteristicNotification(notify,true)) throw new IOException("Cannot enable CyberPi notifications");
        BluetoothGattDescriptor cccd=notify.getDescriptor(uuid(0x2902));
        if(cccd==null) throw new IOException("CyberPi notification descriptor missing");
        if(Build.VERSION.SDK_INT>=33) {
            if(opened.writeDescriptor(cccd,BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)!=BluetoothStatusCodes.SUCCESS)
                throw new IOException("Cannot subscribe to CyberPi");
        } else {
            cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            if(!opened.writeDescriptor(cccd)) throw new IOException("Cannot subscribe to CyberPi");
        }
        await(subscribed);
    }

    @SuppressWarnings("deprecation")
    synchronized void write(byte[] data) throws Exception {
        for(int at=0;at<data.length;at+=20) {
            check();
            byte[] chunk=java.util.Arrays.copyOfRange(data,at,Math.min(at+20,data.length));
            BluetoothGatt current=gatt;
            if(current==null) throw new IOException("CyberPi disconnected");
            int type=(write.getProperties() & BluetoothGattCharacteristic.PROPERTY_WRITE)!=0 ?
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT : BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE;
            writes.clear();
            if(Build.VERSION.SDK_INT>=33) {
                if(current.writeCharacteristic(write,chunk,type)!=BluetoothStatusCodes.SUCCESS) throw new IOException("CyberPi write rejected");
            } else {
                write.setWriteType(type); write.setValue(chunk);
                if(!current.writeCharacteristic(write)) throw new IOException("CyberPi write rejected");
            }
            Integer result=writes.poll(5,TimeUnit.SECONDS);
            check();
            if(result==null || result!=0) throw new IOException("CyberPi write unconfirmed");
        }
    }
    byte[] read(long timeoutMs) throws Exception {
        check();
        byte[] data=timeoutMs==0 ? received.take() : received.poll(timeoutMs,TimeUnit.MILLISECONDS);
        check();
        if(data==null) throw new IOException("CyberPi reply timed out");
        return data;
    }
    void close() {
        closed=true; fail("CyberPi transport closed");
        BluetoothGatt current=gatt; gatt=null;
        if(current!=null) { current.disconnect(); current.close(); }
    }
}
