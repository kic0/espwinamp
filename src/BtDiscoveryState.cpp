#include "BtDiscoveryState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include <SPIFFS.h>
#include "pins.h"

std::vector<DiscoveredBTDevice> BtDiscoveryState::bt_devices;
int BtDiscoveryState::selected_bt_device = 0;
int BtDiscoveryState::bt_discovery_scroll_offset = 0;
volatile bool BtDiscoveryState::is_scanning = false;

void BtDiscoveryState::enter(AppContext& context) {
    esp_bt_gap_register_callback(esp_bt_gap_cb);
}

State* BtDiscoveryState::loop(AppContext& context) {
    // Button handling
    bool current_scroll = !digitalRead(BTN_SCROLL);
    static bool scroll_pressed = false;
    static unsigned long scroll_press_time = 0;
    static bool scroll_long_press_triggered = false;

    if (current_scroll && !scroll_pressed) {
        scroll_pressed = true;
        scroll_press_time = millis();
        scroll_long_press_triggered = false;
    } else if (!current_scroll && scroll_pressed) {
        scroll_pressed = false;
        if (!scroll_long_press_triggered) {
            return handle_button_press(context, true, true);
        }
    }
    if (scroll_pressed && !scroll_long_press_triggered && (millis() - scroll_press_time >= 1000)) {
        scroll_long_press_triggered = true;
        return handle_button_press(context, false, true);
    }

    // Start scanning
    static unsigned long last_scan_time = -12000;
    if (!is_scanning && millis() - last_scan_time > 12000) {
        bt_devices.clear();
        selected_bt_device = 0;
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
        is_scanning = true;
        last_scan_time = millis();
    }

    draw_bt_discovery_ui(context);
    return nullptr;
}

void BtDiscoveryState::exit(AppContext& context) {}

State* BtDiscoveryState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        selected_bt_device++;
        if (selected_bt_device >= bt_devices.size()) {
            selected_bt_device = 0;
        }
        context.ui_dirty = true;
    } else if (is_scroll_button && !is_short_press) {
        if (!bt_devices.empty()) {
            esp_bt_gap_cancel_discovery();
            is_scanning = false;
            context.a2dp.connect_to(bt_devices[selected_bt_device].address);
            return new ArtistSelectionState();
        }
    }
    return nullptr;
}

void BtDiscoveryState::draw_bt_discovery_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);

    context.display.setCursor(0, 0);
    context.display.print("Select BT Speaker:");
    context.display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (bt_devices.empty()) {
        context.display.setCursor(0, 26);
        context.display.print("Scanning...");
    } else {
        for (int i = 0; i < bt_devices.size() && i < 4; i++) {
            int y_pos = 26 + i * 10;
            if (i == selected_bt_device) {
                context.display.setCursor(0, y_pos);
                context.display.print("> ");
                context.display.print(bt_devices[i].name);
            } else {
                context.display.setCursor(12, y_pos);
                context.display.print(bt_devices[i].name);
            }
        }
    }
    context.display.display();
}

void BtDiscoveryState::esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
        DiscoveredBTDevice new_device;
        memcpy(new_device.address, param->disc_res.bda, ESP_BD_ADDR_LEN);

        char *name = NULL;
        for (int i = 0; i < param->disc_res.num_prop; i++) {
            if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
                name = (char *)param->disc_res.prop[i].val;
                new_device.name = String(name);
                break;
            }
        }
        if (name == NULL) {
            char bda_str[18];
            sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x", param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2], param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
            new_device.name = String(bda_str);
        }

        bool found = false;
        for (const auto& dev : bt_devices) {
            if (memcmp(dev.address, new_device.address, ESP_BD_ADDR_LEN) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            bt_devices.push_back(new_device);
        }
    } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            is_scanning = false;
        }
    }
}
