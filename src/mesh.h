#pragma once

#include <cstdint>
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"

class mesh {
public:
    mesh(uint8_t* uuid);

    void init();
    void send_onoff(uint8_t onoff);

private:
    // Device info
    uint8_t* dev_uuid;

    // Mesh state
    uint16_t net_idx;
    uint16_t app_idx;
    uint8_t tid;

    // Internal init
    void ble_mesh_init_internal();

    // --- Callbacks ---
    static void prov_complete(uint16_t net_idx, uint16_t addr,
                              uint8_t flags, uint32_t iv_index);

    static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                         esp_ble_mesh_prov_cb_param_t *param);

    static void ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                           esp_ble_mesh_generic_server_cb_param_t *param);

    static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                          esp_ble_mesh_cfg_server_cb_param_t *param);
};