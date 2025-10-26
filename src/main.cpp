#include "AudioTools.h"
#include "BluetoothA2DPSource.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioLibs/AudioSourceSD.h"
#include "AudioLibs/A2DPStream.h"

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

A2DPStream a2dp_stream;
AudioSourceSD sd_source(file_name, SD_CS);
MP3DecoderHelix decoder;
StreamCopy copier(a2dp_stream, decoder, sd_source);

void connection_state_callback(esp_a2d_connection_state_t state, void* ptr){
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED){
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Connected!");
        display.println("Playing...");
        display.display();
        a2dp_stream.begin(TX_A2DP_I2S);
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

    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        display.println("SD Card Error!");
        display.display();
        while (1);
    }
    display.println("SD Card OK.");
    display.display();
    delay(1000);

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connecting to:");
    display.println(bt_address);
    display.display();

    a2dp_stream.set_on_connection_state_changed(connection_state_callback);
    a2dp_stream.start({bt_address});

    copier.begin();
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
