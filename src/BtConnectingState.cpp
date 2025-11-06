#include "BtConnectingState.h"
#include "AppContext.h"
#include "SamplePlaybackState.h"
#include "Log.h"
#include <SPIFFS.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"


void BtConnectingState::enter(AppContext& context) {
    Log::printf("Entering BT Connecting State\n");
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);
    context.display.setCursor(0, 0);
    context.display.println("Connecting...");
    context.display.display();
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
        return new SamplePlaybackState();
    }
    // Add a timeout eventually
    return nullptr;
}

void BtConnectingState::exit(AppContext& context) {}
