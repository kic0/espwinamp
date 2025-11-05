#include <Arduino.h>
#include "AppContext.h"
#include "StateManager.h"
#include "BtDiscoveryState.h"
#include "SettingsState.h"
#include <Adafruit_SSD1306.h>
#include <BluetoothA2DPSource.h>
#include <SPIFFS.h>
#include <SD.h>
#include "pins.h"
#include <Wire.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
BluetoothA2DPSource a2dp;

StateManager* stateManager;
AppContext* appContext;
extern AppContext* appContext_Audio;

void setup() {
    Serial.begin(115200);

    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    if (!SD.begin(SD_CS)) {
        Serial.println("Card Mount Failed");
        return;
    }

    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    appContext = new AppContext(display, a2dp);
    appContext_Audio = appContext;
    stateManager = new StateManager(*appContext);

    if (SPIFFS.exists("/wifi_mode.txt")) {
        stateManager->setState(new SettingsState());
    } else {
        a2dp.start("winamp");
        stateManager->setState(new BtDiscoveryState());
    }
}

void loop() {
    stateManager->loop();
    delay(50);
}
