#include "BtDiscoveryState.h"
#include "AppContext.h"
#include "BtConnectingState.h"
#include "StateManager.h"
#include "SettingsState.h"
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <SPIFFS.h>
#include "Log.h"
#include "UI.h"

// Initialize static members
std::vector<DiscoveredBTDevice> BtDiscoveryState::bt_devices;
int BtDiscoveryState::selected_bt_device = 0;
volatile bool BtDiscoveryState::is_scanning = false;
esp_bd_addr_t BtDiscoveryState::saved_peer_address = {0};
bool BtDiscoveryState::has_saved_address = false;

// Global pointers for C-style callback
extern AppContext* g_appContext;
extern StateManager* g_stateManager;

void BtDiscoveryState::enter(AppContext& context) {
    Log::printf("Entering BT Discovery State\n");
    stop_audio_playback(context);
    bt_devices.clear();
    selected_bt_device = 0;
    is_scanning = true;

    // Read saved BT address from SPIFFS
    has_saved_address = false;
    if (SPIFFS.exists("/data/bt_address.txt")) {
        File file = SPIFFS.open("/data/bt_address.txt", "r");
        if (file) {
            String mac_str = file.readString();
            if (mac_str.length() == 17) {
                unsigned int mac[6];
                sscanf(mac_str.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
                for (int i=0; i<6; ++i) saved_peer_address[i] = (uint8_t)mac[i];
                has_saved_address = true;
                Log::printf("Loaded saved BT address: %s\n", mac_str.c_str());
            }
            file.close();
        }
    }

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
        is_scanning = false; // Ensure we don't restart discovery
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
    } else { // Long press for manual connection
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
            // Check if device is already in our list
            for (auto const& device : bt_devices) {
                if (memcmp(device.address, param->disc_res.bda, ESP_BD_ADDR_LEN) == 0) {
                    return; // Already found
                }
            }

            DiscoveredBTDevice new_device;
            memcpy(new_device.address, param->disc_res.bda, ESP_BD_ADDR_LEN);

            char bda_str[18];
            sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
                    param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
            Log::printf("Device found: %s\n", bda_str);

            // Extract name from properties
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

            // Auto-connect logic
            if (has_saved_address && memcmp(new_device.address, saved_peer_address, ESP_BD_ADDR_LEN) == 0) {
                Log::printf("Found saved device, attempting to connect...\n");
                memcpy(g_appContext->peer_address, new_device.address, sizeof(esp_bd_addr_t));
                esp_bt_gap_cancel_discovery();
                is_scanning = false;
                g_stateManager->setState(new BtConnectingState());
            }
            break;
        }
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                if (is_scanning) { // Only restart if we didn't intentionally stop it
                    Log::printf("Discovery stopped. Restarting...\n");
                    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
                } else {
                    Log::printf("Discovery stopped intentionally.\n");
                }
            } else {
                 Log::printf("Discovery started.\n");
            }
            break;
        }
        default:
            break;
    }
}
