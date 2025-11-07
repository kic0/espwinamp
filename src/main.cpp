#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPIFFS.h>
#include <SD.h>
#include <BluetoothA2DPSource.h>
#include "AppContext.h"
#include "StateManager.h"
#include "BtDiscoveryState.h"
#include "pins.h"
#include "Log.h"
#include "UI.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
BluetoothA2DPSource a2dp;
Button button(BTN_SCROLL);
AppContext context(display, a2dp, button);
StateManager stateManager(context);

// Global pointer for C-style callbacks
AppContext* g_appContext = &context;
extern StateManager* g_stateManager; // For callback

void bt_connection_state_cb(esp_a2d_connection_state_t state, void *ptr) {
    Log::printf("BT Connection State Changed: %d\n", state);
    if (g_appContext) {
        g_appContext->is_bt_connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
        if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            // Save playback position
            if (g_appContext->audioFile) {
                g_appContext->playback_position = g_appContext->audioFile.position();
            }
            g_stateManager->requestStateChange(new BtDiscoveryState());
        }
    }
}

void setup() {
    Serial.begin(115200);
    Log::printf("Setup starting...\n");
    delay(1000); // Power stabilization

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Log::printf("SSD1306 allocation failed\n");
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.print("Initializing...");
    display.display();

    SPIFFS.begin(true);

    if(!SD.begin(SD_CS)){
        Log::printf("Card Mount Failed\n");
        display.clearDisplay();
        display.setCursor(0,0);
        display.print("SD Card Error!");
        display.display();
        delay(2000);
        ESP.restart();
    }

    a2dp.set_on_connection_state_changed(bt_connection_state_cb);
    a2dp.start("winamp");

    context.state_manager = &stateManager;
    stateManager.setState(new BtDiscoveryState());
}

void loop() {
    stateManager.loop();

    State* currentState = stateManager.getCurrentState();
    if (currentState && context.ui_dirty) {
        switch(currentState->getType()) {
            case StateType::ARTIST_SELECTION:
                draw_list_ui(context, "Artists", context.artists, context.selected_artist, "-> Settings");
                break;
            case StateType::PLAYLIST_SELECTION:
                draw_list_ui(context, "Playlists", context.playlists, context.selected_playlist, "<- back");
                break;
            case StateType::PLAYER:
                draw_player_ui(context);
                break;
            case StateType::SETTINGS:
                draw_settings_ui(context);
                break;
            case StateType::BT_DISCOVERY:
                draw_bt_discovery_ui(context);
                break;
            default:
                break;
        }
    }
}
