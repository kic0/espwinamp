#include "BtAutoConnectingState.h"
#include "AppContext.h"
#include "SamplePlaybackState.h"
#include "BtDiscoveryState.h"
#include "Log.h"

void BtAutoConnectingState::enter(AppContext& context) {
    Log::printf("Entering BT Auto-Connecting State\n");
    start_time = millis();
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);
    context.display.setCursor(0, 0);
    context.display.println("Connecting...");
    context.display.display();
}

State* BtAutoConnectingState::loop(AppContext& context) {
    if (context.is_bt_connected) {
        return new SamplePlaybackState();
    }
    if (millis() - start_time > 10000) {
        return new BtDiscoveryState();
    }
    return nullptr;
}

void BtAutoConnectingState::exit(AppContext& context) {}
