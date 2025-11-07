#ifndef BT_DISCOVERY_STATE_H
#define BT_DISCOVERY_STATE_H

#include "State.h"
#include "esp_gap_bt_api.h"
#include <vector>
#include <Arduino.h>

struct DiscoveredBTDevice {
    String name;
    esp_bd_addr_t address;
};

class BtDiscoveryState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;
    StateType getType() const override { return StateType::BT_DISCOVERY; }

    static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

    // Public getters for UI
    const std::vector<DiscoveredBTDevice>& get_discovered_devices() const { return bt_devices; }
    int get_selected_device() const { return selected_bt_device; }

private:
    State* handle_button_press(AppContext& context, bool is_short_press);

    static std::vector<DiscoveredBTDevice> bt_devices;
    static int selected_bt_device;
    static volatile bool is_scanning;
    static esp_bd_addr_t saved_peer_address; // To hold address from SPIFFS
    static bool has_saved_address;
};

#endif // BT_DISCOVERY_STATE_H
