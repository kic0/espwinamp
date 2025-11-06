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
            const uint8_t* addr = esp_bt_dev_get_address();
            sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
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
