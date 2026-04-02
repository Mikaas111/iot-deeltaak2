#pragma once

#include <cstdint>
#include <functional>
#include "esp_err.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"

class mesh {
public:
    using onoff_callback_t = std::function<void(uint8_t onoff)>;

    explicit mesh(uint8_t *uuid);

    void init();
    void send_onoff(uint8_t onoff);
    bool is_provisioned() const;
    void set_onoff_callback(onoff_callback_t cb);

private:
    uint8_t *dev_uuid;
    uint16_t net_idx;
    uint16_t app_idx;
    bool provisioned;
    uint8_t tid;

    onoff_callback_t onoff_callback_;

    void ble_mesh_init_internal();

    static void prov_complete(uint16_t net_idx, uint16_t addr,
                              uint8_t flags, uint32_t iv_index);

    static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                         esp_ble_mesh_prov_cb_param_t *param);

    static void ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                           esp_ble_mesh_generic_server_cb_param_t *param);

    static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                          esp_ble_mesh_cfg_server_cb_param_t *param);

    static void ble_mesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                           esp_ble_mesh_generic_client_cb_param_t *param);
};