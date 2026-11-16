// SPDX-License-Identifier: GPL-3.0-only

#include "ble_nus.h"
#include "wire.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "esp_timer.h"

#include "host/ble_att.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"
#include "os/os_mbuf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

void ble_store_config_init(void); // NimBLE store/config initializer (not declared in its public header).
static const char *TAG = "nus";
static uint8_t own_addr_type;
static uint16_t connection = BLE_HS_CONN_HANDLE_NONE, tx_handle;
static bool subscribed;
static uint8_t frame_id;
static portMUX_TYPE pair_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t pairing_until = 0;
void ble_nus_pairing_window(unsigned seconds)
{
    if (seconds > 60)
        seconds = 60;
    portENTER_CRITICAL(&pair_lock);
    pairing_until = esp_timer_get_time() + (int64_t)seconds * 1000000;
    portEXIT_CRITICAL(&pair_lock);
    ESP_LOGI(TAG, "Physical-key pairing window: %u seconds", seconds);
}
static bool pairing_open(void)
{
    portENTER_CRITICAL(&pair_lock);
    int64_t until = pairing_until;
    portEXIT_CRITICAL(&pair_lock);
    return esp_timer_get_time() < until;
}
static bool known_peer(uint16_t handle)
{
    struct ble_gap_conn_desc d;
    if (ble_gap_conn_find(handle, &d))
        return false;
    ble_addr_t peers[16];
    int count = 0;
    if (ble_store_util_bonded_peers(peers, &count, 16))
        return false;
    for (int i = 0; i < count; ++i)
        if (peers[i].type == d.peer_id_addr.type && memcmp(peers[i].val, d.peer_id_addr.val, 6) == 0)
            return true;
    return false;
}
static SemaphoreHandle_t publish_mutex;
static const ble_uuid128_t service_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t rx_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t tx_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);
static int access_rx(uint16_t conn, uint16_t handle, struct ble_gatt_access_ctxt *ctx, void *arg)
{
    (void)conn;
    (void)handle;
    (void)arg;
    uint8_t data[244];
    uint16_t size = 0;
    if (ble_hs_mbuf_to_flat(ctx->om, data, sizeof data, &size) != 0 || size < 1)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(conn, &desc) || !desc.sec_state.encrypted || !desc.sec_state.bonded)
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    if (data[0] == 0xf1 && size == 5)
    {
        uint32_t id = (uint32_t)data[1] | ((uint32_t)data[2] << 8) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);
        return telemetry_link_info(id) == 0 ? 0 : BLE_ATT_ERR_UNLIKELY;
    }
    if (size < 4)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (data[0] == 2 || data[0] == 4 || data[0] == 8 || data[0] == 9)
    {
        if (size < 6)
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        const unsigned n = data[1] | (data[2] << 8);
        if (n != size - 3 || data[3] != data[0])
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        int rc = data[0] == 9 ? telemetry_generic_submit(data + 4, size - 4) : data[0] == 2 ? telemetry_control_submit(data + 4, size - 4) : data[0] == 4 ? telemetry_remote_submit(data + 4, size - 4)
                                                                                            : telemetry_color_calibration_submit(data + 4, size - 4);
        return rc == 0 ? 0 : BLE_ATT_ERR_UNLIKELY;
    }
    if (data[0] != 0xf0)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    const unsigned a = data[1];
    if (a == 0 || a > 32 || 2 + a >= size)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    const unsigned b = data[2 + a];
    if (b > 63 || 3 + a + b != size)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    return telemetry_wifi_provision(data + 2, a, data + 3 + a, b) == 0 ? 0 : BLE_ATT_ERR_UNLIKELY;
}
static const struct ble_gatt_svc_def svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &service_uuid.u, .characteristics = (struct ble_gatt_chr_def[]){{.uuid = &rx_uuid.u, .access_cb = access_rx, .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC}, {.uuid = &tx_uuid.u, .val_handle = &tx_handle, .flags = BLE_GATT_CHR_F_NOTIFY}, {0}}}, {0}};
static void advertise(void);
static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            connection = event->connect.conn_handle;
            if (!known_peer(connection) && !pairing_open())
            {
                ble_gap_terminate(connection, BLE_ERR_REM_USER_CONN_TERM);
                break;
            }
            (void)ble_gap_security_initiate(connection);
        }
        else
            advertise();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        connection = BLE_HS_CONN_HANDLE_NONE;
        subscribed = false;
        advertise();
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // A phone may replace a lost native bond only during a physical-key window.
        if (!pairing_open())
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        {
            struct ble_gap_conn_desc d;
            if (!ble_gap_conn_find(event->repeat_pairing.conn_handle, &d))
                ble_store_util_delete_peer(&d.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == tx_handle)
            subscribed = event->subscribe.cur_notify != 0;
        break;
    default:
        break;
    }
    return 0;
}
static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)"AXI-EKF";
    fields.name_len = 7;
    fields.name_is_complete = 1;
    fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    /* Name + 128-bit UUID may exceed 31-byte legacy advertisement; publish name in scan response. */
    fields.name = NULL;
    fields.name_len = 0;
    if (ble_gap_adv_set_fields(&fields))
        return;
    struct ble_hs_adv_fields scan = {0};
    scan.name = (const uint8_t *)"AXI-EKF";
    scan.name_len = 7;
    scan.name_is_complete = 1;
    if (ble_gap_adv_rsp_set_fields(&scan))
        return;
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    (void)ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
}
static void on_sync(void)
{
    if (ble_hs_id_infer_auto(0, &own_addr_type) == 0)
        advertise();
}
static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
int ble_nus_start(void)
{
    publish_mutex = xSemaphoreCreateMutex();
    if (!publish_mutex)
        return -1;
    if (nimble_port_init() != 0)
        return -1;
    (void)ble_att_set_preferred_mtu(247);
    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (ble_svc_gap_device_name_set("AXI-EKF") != 0 ||
        ble_gatts_count_cfg(svcs) != 0 || ble_gatts_add_svcs(svcs) != 0)
        return -1;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_store_config_init();
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "NUS started; pair before provisioning");
    return 0;
}
int ble_nus_publish(uint8_t kind, const uint8_t *proto, size_t len)
{
    if (!proto || !len || len > AXI_MAX_PROTO || !publish_mutex)
        return -1;
    if (xSemaphoreTake(publish_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return -1;
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(connection, &desc) || !desc.sec_state.encrypted || !desc.sec_state.bonded)
    {
        xSemaphoreGive(publish_mutex);
        return -1;
    }
    if (!subscribed || connection == BLE_HS_CONN_HANDLE_NONE)
    {
        xSemaphoreGive(publish_mutex);
        return -1;
    }
    const uint16_t mtu = ble_att_mtu(connection);
    const size_t budget = mtu > 7 ? (size_t)mtu - 7 : 0; /* ATT 3 + chunk header 4 */
    if (budget < 2)
    {
        xSemaphoreGive(publish_mutex);
        return -1;
    }
    uint8_t bytes[AXI_MAX_PROTO + 3];
    bytes[0] = (uint8_t)(len + 1);
    bytes[1] = (uint8_t)((len + 1) >> 8);
    bytes[2] = kind;
    memcpy(bytes + 3, proto, len);
    len += 3;
    const unsigned count = (unsigned)((len + budget - 1) / budget);
    if (!count || count > 255)
    {
        xSemaphoreGive(publish_mutex);
        return -1;
    }
    const uint8_t id = ++frame_id;
    int result = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        const size_t offset = i * budget, take = len - offset < budget ? len - offset : budget;
        uint8_t chunk[AXI_MAX_PROTO + 6] = {0xb7, id, (uint8_t)i, (uint8_t)count};
        memcpy(chunk + 4, bytes + offset, take);
        struct os_mbuf *mb = ble_hs_mbuf_from_flat(chunk, take + 4);
        if (!mb || ble_gatts_notify_custom(connection, tx_handle, mb) != 0)
        {
            result = -1;
            break;
        }
    }
    xSemaphoreGive(publish_mutex);
    return result;
}
