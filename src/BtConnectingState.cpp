#include "BtConnectingState.h"
#include "AppContext.h"
#include "SamplePlaybackState.h"
#include "Log.h"

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
        return new SamplePlaybackState();
    }
    // Add a timeout eventually
    return nullptr;
}

void BtConnectingState::exit(AppContext& context) {}
