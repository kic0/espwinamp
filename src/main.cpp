#include "AudioTools.h"
#include "BluetoothA2DPSource.h"
#include "arduino-audio-tools/src/AudioTools/AudioCodecs/CodecMP3Mini.h"
#include "arduino-audio-tools/src/AudioTools/Disk/AudioSourceSD.h"
#include "arduino-audio-tools/src/AudioTools/AudioLibs/A2DPStream.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>

// Pin Definitions
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST 16
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SD_CS   13
#define SD_SCLK 14
#define SD_MOSI 15
#define SD_MISO 2

// Global Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
const char *bt_address = "CA:AE:57:5D:DF:1C";
const char *file_name = "/data/sample.mp3";

BluetoothA2DPSource a2dp_source;
A2DPStream a2dp_out;
AudioSourceSD sd_source;
MP3Decoder decoder;
StreamCopy copier;

void connection_state_callback(esp_a2d_connection_state_t state, void* ptr){
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED){
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Connected!");
        display.println("Playing...");
        display.display();
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED){
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Disconnected");
        display.println("Rebooting...");
        display.display();
        delay(3000);
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("ESP32 MP3 Player");
    display.display();
    delay(1000);

    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
    SD.begin(SD_CS);
    sd_source.open(file_name);
    display.println("SD Card OK.");
    display.display();
    delay(1000);

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connecting to:");
    display.println(bt_address);
    display.display();

    a2dp_out.begin(a2dp_source);
    a2dp_source.set_on_connection_state_changed(connection_state_callback);
    a2dp_source.start({bt_address});

    copier.begin(a2dp_out, decoder, sd_source);
}

void loop() {
    if (!copier.copy()) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Finished");
        display.println("Rebooting...");
        display.display();
        delay(3000);
        ESP.restart();
    }
}
