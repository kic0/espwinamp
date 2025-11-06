#include <Arduino.h>
#include "AppContext.h"
#include "StateManager.h"
#include "BtDiscoveryState.h"
#include "SettingsState.h"
#include "BtConnectingState.h"
#include <Adafruit_SSD1306.h>
#include <BluetoothA2DPSource.h>
#include <SPIFFS.h>
#include <SD.h>
#include "pins.h"
#include <Wire.h>
#include "Button.h"
#include "Log.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
BluetoothA2DPSource a2dp;
Button button(BTN_SCROLL);

StateManager* stateManager;
StateManager* g_stateManager = nullptr; // Global StateManager pointer
AppContext* appContext;
AppContext* g_appContext = nullptr; // Global AppContext pointer

void bt_connection_state_cb(esp_a2d_connection_state_t state, void* ptr){
    Log::printf("A2DP connection state changed: %d\n", state);
    if (g_appContext) {
        g_appContext->is_bt_connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
        if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            if (g_appContext->audioFile) {
                g_appContext->playback_position = g_appContext->audioFile.position();
                g_appContext->audioFile.close();
            }
            stateManager->setState(new BtDiscoveryState());
        }
    }
}

void setup() {
    Serial.begin(115200);
    Log::printf("--- ESPWinamp Starting ---\n");

    Log::printf("Initializing Display...\n");
    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Log::printf("SSD1306 allocation failed\n");
        for(;;);
    }

    Log::printf("Initializing SD Card...\n");
    if (!SD.begin(SD_CS)) {
        Log::printf("Card Mount Failed\n");
        return;
    }

    Log::printf("Initializing SPIFFS...\n");
    if(!SPIFFS.begin(true)){
        Log::printf("An Error has occurred while mounting SPIFFS\n");
        return;
    }

    button.begin();

    appContext = new AppContext(display, a2dp, button);
    g_appContext = appContext;
    stateManager = new StateManager(*appContext);
    g_stateManager = stateManager;

    if (SPIFFS.exists("/wifi_mode.txt")) {
        Log::printf("Booting into WiFi Mode\n");
        stateManager->setState(new SettingsState());
    } else {
        Log::printf("Booting into Bluetooth Mode\n");
        a2dp.set_on_connection_state_changed(bt_connection_state_cb);
        a2dp.start("winamp");
        stateManager->setState(new BtDiscoveryState());
    }
}

void loop() {
    static unsigned long last_heap_log = 0;
    if (millis() - last_heap_log > 2000) {
        Log::printf("Free heap: %d bytes | Decoder: sample_rate=%d, bps=%d, channels=%d\n",
                      ESP.getFreeHeap(), appContext->diag_sample_rate, appContext->diag_bits_per_sample, appContext->diag_channels);
        last_heap_log = millis();
    }

    stateManager->loop();
    delay(50);
}
