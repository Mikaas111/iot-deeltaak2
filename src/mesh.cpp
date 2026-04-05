#include "mesh.h"
#include "led.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"

#define CID_ESP 0x02E5
#define GROUP_ADDR 0xC000

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

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_srv, 2 + 3, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_cli, 2 + 3, ROLE_NODE);

static esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

static esp_ble_mesh_client_t onoff_client = {};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_srv, &onoff_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_pub_cli, &onoff_client),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

static esp_ble_mesh_prov_t provision = {};
static mesh *g_mesh = nullptr;

mesh::mesh(uint8_t *uuid)
    : dev_uuid(uuid), net_idx(0xFFFF), app_idx(0xFFFF), provisioned(false), tid(0)
{
    provision.uuid = dev_uuid;
    provision.output_size = 0;
    provision.output_actions = 0;
}

bool mesh::is_provisioned() const
{
    return provisioned;
}

void mesh::set_onoff_callback(onoff_callback_t cb)
{
    onoff_callback_ = std::move(cb);
}

void mesh::init()
{
    g_mesh = this;

    esp_ble_mesh_register_prov_callback(mesh::ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(mesh::ble_mesh_config_server_cb);
    esp_ble_mesh_register_generic_server_callback(mesh::ble_mesh_generic_server_cb);
    esp_ble_mesh_register_generic_client_callback(mesh::ble_mesh_generic_client_cb);

    ble_mesh_init_internal();
}

void mesh::ble_mesh_init_internal()
{
    esp_ble_mesh_init(&provision, &composition);

    esp_ble_mesh_node_prov_enable(
        (esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
}

void mesh::send_onoff(uint8_t onoff)
{
    setLed(onoff ? LED_ON : LED_OFF);

    if (!g_mesh || !g_mesh->provisioned) {
        return;
    }

    esp_ble_mesh_client_common_param_t common = {};
    esp_ble_mesh_generic_client_set_state_t set = {};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
    common.model = &root_models[2];
    common.ctx.net_idx = g_mesh->net_idx;
    common.ctx.app_idx = g_mesh->app_idx;
    common.ctx.addr = GROUP_ADDR;
    common.ctx.send_ttl = 3;
    common.msg_timeout = 0;

    set.onoff_set.onoff = onoff;
    set.onoff_set.op_en = false;
    set.onoff_set.tid = tid++;

    esp_ble_mesh_generic_client_set_state(&common, &set);
}

void mesh::prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    if (!g_mesh) return;

    g_mesh->net_idx = net_idx;
    g_mesh->app_idx = 0x0000;
    g_mesh->provisioned = true;

    setLed(LED_OFF);
}

void mesh::ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                    esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        prov_complete(param->node_prov_complete.net_idx,
                      param->node_prov_complete.addr,
                      param->node_prov_complete.flags,
                      param->node_prov_complete.iv_index);
        break;
    default:
        break;
    }
}

void mesh::ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                      esp_ble_mesh_generic_server_cb_param_t *param)
{
    if (event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT) return;

    if (param->ctx.recv_op != ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET &&
        param->ctx.recv_op != ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
        return;
    }

    uint8_t onoff = param->value.state_change.onoff_set.onoff;

    setLed(onoff ? LED_ON : LED_OFF);

    if (g_mesh && param->ctx.addr == g_mesh->net_idx) {
        return;
    }

    if (g_mesh && g_mesh->onoff_callback_) {
        g_mesh->onoff_callback_(onoff);
    }
}

void mesh::ble_mesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t,
                                      esp_ble_mesh_generic_client_cb_param_t *)
{
}

void mesh::ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                     esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event != ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) return;

    switch (param->ctx.recv_op) {
    case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
        if (param->value.state_change.mod_app_bind.company_id == 0xFFFF &&
            param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI) {
            if (g_mesh) {
                g_mesh->app_idx = param->value.state_change.mod_app_bind.app_idx;
            }
        }
        break;

    default:
        break;
    }
}