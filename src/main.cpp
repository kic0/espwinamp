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

AppContext* g_appContext = &context;
extern StateManager* g_stateManager;

void audioTask(void* parameter);

void bt_connection_state_cb(esp_a2d_connection_state_t state, void *ptr) {
    Log::printf("BT Connection State Changed: %d\n", state);
    if (g_appContext) {
        g_appContext->is_bt_connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
        if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
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
    delay(1000);

    if(!SD.begin(SD_CS)){
        Log::printf("Card Mount Failed. Restarting...\n");
        delay(2000);
        ESP.restart();
    }
    Log::printf("SD Card initialized.\n");
    delay(500);

    a2dp.set_on_connection_state_changed(bt_connection_state_cb);
    a2dp.start("winamp");
    Log::printf("Bluetooth initialized.\n");
    delay(500);

    Wire.begin(OLED_SDA, OLED_SCL);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Log::printf("SSD1306 allocation failed\n");
    } else {
        Log::printf("Display initialized.\n");
    }

    SPIFFS.begin(true);

    context.audio_task_semaphore = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(
        audioTask, "AudioTask", 4096, NULL, 1, &context.audioTaskHandle, 1
    );

    context.state_manager = &stateManager;
    stateManager.setState(new BtDiscoveryState());
}

unsigned long last_heap_check = 0;

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
            case StateType::SAMPLE_PLAYBACK:
                draw_sample_playback_ui(context);
                break;
            case StateType::BT_CONNECTING:
                draw_connecting_ui(context);
                break;
            default:
                break;
        }
        context.ui_dirty = false;
    }

    if (millis() - last_heap_check > 2000) {
        Log::printf("Free heap: %u bytes\n", ESP.getFreeHeap());
        last_heap_check = millis();
    }
}
