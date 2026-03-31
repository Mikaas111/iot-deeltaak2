#include "mesh.h"
#include "board.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#define TAG "MESH_NODE"
#define CID_ESP 0x02E5

extern struct _led_state led_state[3];

// ================= CONFIG =================

static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub, 2 + 3, ROLE_NODE);

static esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

// ⚠️ BELANGRIJK: model pointer nodig voor client send
static esp_ble_mesh_model_t *onoff_model_ptr = nullptr;

// ================= COMPOSITION =================

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub, &onoff_server),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

static esp_ble_mesh_prov_t provision;

// ================= CLASS =================

mesh::mesh(uint8_t *uuid)
    : dev_uuid(uuid), net_idx(0xFFFF), app_idx(0xFFFF), tid(0)
{
    provision.uuid = dev_uuid;
    provision.output_size = 0;
    provision.output_actions = 0;
}

void mesh::init()
{
    esp_ble_mesh_register_prov_callback(mesh::ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(mesh::ble_mesh_config_server_cb);
    esp_ble_mesh_register_generic_server_callback(mesh::ble_mesh_generic_server_cb);

    ble_mesh_init_internal();

    // Save pointer to model for later use
    onoff_model_ptr = &root_models[1];

    board_led_operation(LED_G, LED_ON);
}

void mesh::ble_mesh_init_internal()
{
    esp_err_t err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Mesh init failed (err %d)", err);
        return;
    }

    esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV);
    esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_GATT);

    ESP_LOGI(TAG, "BLE Mesh Node initialized");
}

// ================= SEND =================

void mesh::send_onoff(uint8_t onoff)
{
    if (!onoff_model_ptr) {
        ESP_LOGE(TAG, "Model not initialized");
        return;
    }

    esp_ble_mesh_client_common_param_t common = {};
    esp_ble_mesh_generic_client_set_state_t set = {};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
    common.model = onoff_model_ptr;
    common.ctx.net_idx = net_idx;
    common.ctx.app_idx = app_idx;
    common.ctx.addr = 0xFFFF;
    common.ctx.send_ttl = 3;
    common.msg_timeout = 0;

    set.onoff_set.onoff = onoff;
    set.onoff_set.op_en = false;
    set.onoff_set.tid = tid++;

    esp_err_t err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err) {
        ESP_LOGE(TAG, "Send OnOff failed (err %d)", err);
    }
}

// ================= CALLBACKS =================

void mesh::prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "Provision complete: addr 0x%04x", addr);
}

void mesh::ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                    esp_ble_mesh_prov_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT) {
        mesh::prov_complete(param->node_prov_complete.net_idx,
                            param->node_prov_complete.addr,
                            param->node_prov_complete.flags,
                            param->node_prov_complete.iv_index);
    }
}

void mesh::ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                      esp_ble_mesh_generic_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT) {

        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK)
        {
            uint16_t primary_addr = esp_ble_mesh_get_primary_element_address();

            if (param->ctx.addr != primary_addr) {
                board_led_operation(LED_R,
                    param->value.state_change.onoff_set.onoff);
            }
        }
    }
}

void mesh::ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                     esp_ble_mesh_cfg_server_cb_param_t *param)
{
    // optional
}