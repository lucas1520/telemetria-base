#include "common.h"
#include "host/ble_uuid.h"
#include <stdint.h>
#include "telemetry.h"
#include "gatt_svc.h"
#include "types.h"

static const ble_uuid128_t telemetry_svc_uuid = BLE_UUID128_INIT(
    0x6f, 0x36, 0x9e, 0x45,
    0x4b, 0xf5, 0x4b, 0x44,
    0x9a, 0xae, 0x10, 0x7f,
    0xb0, 0x54, 0x9b, 0x70);

char telemetry_chr_val[64] = {0};
static uint16_t telemetry_chr_val_handle;
static const ble_uuid128_t telemetry_chr_uuid = BLE_UUID128_INIT(
    0x6f, 0x36, 0x9e, 0x45,
    0x4b, 0xf5, 0x4b, 0x44,
    0x9a, 0xae, 0x10, 0x7f,
    0xb0, 0x54, 0x9b, 0x71);

static uint16_t telemetry_chr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool telemetry_chr_conn_handle_inited = false;
static bool telemetry_ind_status = false;

static int telemetry_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* Telemetry service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &telemetry_svc_uuid.u,
        .characteristics = 
            (struct ble_gatt_chr_def[]) {
                {
                    .uuid = &telemetry_chr_uuid.u,
                    .access_cb = telemetry_chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                    .val_handle = &telemetry_chr_val_handle
                },
                {
                    0
                }

            }
    },
    { 0 }
    // TODO make service for BLE TX/RX
};

static int telemetry_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
                ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
            } else {
                ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d", attr_handle);
            }

            if (attr_handle == telemetry_chr_val_handle) {
                // TODO make telemetry mock
                // telemetry_chr_val = get_telemetry_packet();
                strcpy(telemetry_chr_val, get_telemetry_packet());
                // rc = os_mbuf_append(ctxt->om, &telemetry_chr_val, sizeof(telemetry_chr_val));
                rc = os_mbuf_append(
                    ctxt->om,
                    telemetry_chr_val,
                    strlen(telemetry_chr_val)
                );
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            goto error;

        default:
            goto error;
    }
error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to telemetry rate characteristic, opcode: %d",
        ctxt->op
    );
    return BLE_ATT_ERR_UNLIKELY;

}

void send_telemetry_indication(void) {
    if (telemetry_ind_status && telemetry_chr_conn_handle_inited) {
        int rc = ble_gatts_indicate(telemetry_chr_conn_handle,
                                    telemetry_chr_val_handle);
        if (rc != 0) {
            ESP_LOGE(TAG, "failed to send heart rate indication, error code: %d", rc);
        }
    }
}

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    /* Unknown event */
    default:
        assert(0);
        break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update heart rate subscription status
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    /* Check attribute handle */
    if (event->subscribe.attr_handle == telemetry_chr_val_handle) {
        if (event->subscribe.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            return;
        }

        /* Update heart rate subscription status */
        telemetry_chr_conn_handle = event->subscribe.conn_handle;
        telemetry_chr_conn_handle_inited = event->subscribe.cur_indicate;
        telemetry_ind_status = event->subscribe.cur_indicate;
    }
}

void gatt_svr_reset_telemetry_subscription(void) {
    telemetry_chr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    telemetry_chr_conn_handle_inited = false;
    telemetry_ind_status = false;
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void) {
    int rc = 0;

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
