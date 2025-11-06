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

    static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

private:
    void draw_bt_discovery_ui(AppContext& context);
    State* handle_button_press(AppContext& context, bool is_short_press);

    static std::vector<DiscoveredBTDevice> bt_devices;
    static int selected_bt_device;
    static volatile bool is_scanning;
};

#endif // BT_DISCOVERY_STATE_H
