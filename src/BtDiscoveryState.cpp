#include "BtDiscoveryState.h"
#include "AppContext.h"
#include "BtConnectingState.h"
#include "SettingsState.h"
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include "Log.h"
#include "UI.h"

std::vector<DiscoveredBTDevice> BtDiscoveryState::bt_devices;
int BtDiscoveryState::selected_bt_device = 0;
volatile bool BtDiscoveryState::is_scanning = false;

extern AppContext* g_appContext;

void BtDiscoveryState::enter(AppContext& context) {
    Log::printf("Entering BT Discovery State\n");
    stop_audio_playback(context);
    bt_devices.clear();
    selected_bt_device = 0;
    is_scanning = true;

    esp_bt_gap_register_callback(esp_bt_gap_cb);
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}

State* BtDiscoveryState::loop(AppContext& context) {
    ButtonPress press = context.button.read();
    if (press == SHORT_PRESS) {
        return handle_button_press(context, true);
    } else if (press == LONG_PRESS) {
        return handle_button_press(context, false);
    }

    for (int i = 0; i < AppContext::MAX_MARQUEE_LINES; i++) {
        if (context.is_marquee_active[i]) {
            context.ui_dirty = true;
            break;
        }
    }

    // Drawing is now handled in the main loop
    return nullptr;
}

void BtDiscoveryState::exit(AppContext& context) {
    if (is_scanning) {
        esp_bt_gap_cancel_discovery();
    }
    esp_bt_gap_register_callback(nullptr);
}

State* BtDiscoveryState::handle_button_press(AppContext& context, bool is_short_press) {
    if (is_short_press) {
        selected_bt_device++;
        if (selected_bt_device > bt_devices.size()) {
            selected_bt_device = 0;
        }
        context.ui_dirty = true;
    } else { // Long press
        if (selected_bt_device == bt_devices.size()) {
            return new SettingsState();
        } else if (!bt_devices.empty()) {
            memcpy(context.peer_address, bt_devices[selected_bt_device].address, sizeof(esp_bd_addr_t));
            return new BtConnectingState();
        }
    }
    return nullptr;
}

void BtDiscoveryState::esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT: {
            for (auto const& device : bt_devices) {
                bool match = true;
                for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
                    if (device.address[i] != param->disc_res.bda[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) return;
            }

            DiscoveredBTDevice new_device;
            memcpy(new_device.address, param->disc_res.bda, ESP_BD_ADDR_LEN);

            char bda_str[18];
            sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
                    param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);

            Log::printf("Device found: %s\n", bda_str);

            String name = "";
            for (int i = 0; i < param->disc_res.num_prop; i++) {
                if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
                    name = String((char*)param->disc_res.prop[i].val);
                    break;
                }
            }
            new_device.name = (name.length() > 0) ? name : String(bda_str);

            bt_devices.push_back(new_device);
            if (g_appContext) g_appContext->ui_dirty = true;
            break;
        }
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                Log::printf("Discovery stopped.\n");
                is_scanning = false;
            } else {
                 Log::printf("Discovery started.\n");
            }
            break;
        }
        default:
            break;
    }
}
