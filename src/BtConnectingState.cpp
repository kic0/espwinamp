#include "BtConnectingState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include "BtDiscoveryState.h"
#include "Log.h"
#include <SPIFFS.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"


void BtConnectingState::enter(AppContext& context) {
    Log::printf("Entering BT Connecting State\n");
    entry_time = millis();
    context.ui_dirty = true;

    context.a2dp.connect_to(context.peer_address);
}

State* BtConnectingState::loop(AppContext& context) {
    if (context.is_bt_connected) {
        File file = SPIFFS.open("/data/bt_address.txt", "w");
        if (file) {
            char bda_str[18];
            sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x", context.peer_address[0], context.peer_address[1], context.peer_address[2], context.peer_address[3], context.peer_address[4], context.peer_address[5]);
            file.print(bda_str);
            file.close();
            Log::printf("Saved BT address: %s\n", bda_str);
        } else {
            Log::printf("Failed to open /data/bt_address.txt for writing\n");
        }
        return new ArtistSelectionState(); // Go directly to Artist Selection
    }

    if (millis() - entry_time > 10000) {
        Log::printf("Connection timed out.\n");
        context.a2dp.disconnect();
        return new BtDiscoveryState();
    }

    if (!context.is_bt_connected && (millis() - entry_time > 1000)) {
        if(context.a2dp.get_connection_state() == ESP_A2D_CONNECTION_STATE_DISCONNECTED){
             Log::printf("Connection failed, returning to discovery.\n");
             return new BtDiscoveryState();
        }
    }

    return nullptr;
}

void BtConnectingState::exit(AppContext& context) {}
