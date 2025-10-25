/*
  ESP32 MP3 Player for LILYGO T3 LoRa32 V1.6.1

  Description:
  This sketch creates a simple, automated MP3 player. On startup, it initializes
  an SD card and an OLED display. It then attempts to connect to a hard-coded
  Bluetooth headphone MAC address. Once connected, it automatically finds and
  streams a specific MP3 file (`/data/sample.mp3`) from the SD card to the
  headphones. Status updates (initializing, connecting, playing, errors) are
  shown on the OLED display.

  Hardware:
  - LILYGO T3 LoRa32 V1.6.1 Board (ESP32)
  - SD Card Reader connected to dedicated SPI pins
  - Bluetooth Headphones
*/

// I2C and OLED Display Libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Standard ESP32 Libraries
#include <SPI.h>
#include "SD.h"

// Bluetooth and MP3 Libraries
#include "BluetoothA2DPSource.h"
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

// --- Hardware Pin Definitions for LILYGO T3 LoRa32 V1.6.1 ---
// OLED Display
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST 16 // Not strictly needed for all OLEDs, but good practice
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// SD Card (Dedicated Pins)
#define SD_CS   13
#define SD_SCLK 14
#define SD_MOSI 15
#define SD_MISO 2

// --- Global Objects ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
BluetoothA2DPSource a2dp_source;

// --- Bluetooth Connection ---
const char *bt_address = "CA:AE:57:5D:DF:1C";
bool is_playing = false;

// --- MP3 File ---
File mp3File;
const char *mp3FileName = "/data/sample.mp3";

// --- MP3 Decoder ---
mp3dec_t mp3d;
mp3dec_frame_info_t info;
unsigned char mp3_buffer[2048];
short pcm_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];


void setup() {
  // Start Serial for debugging
  Serial.begin(115200);

  // --- OLED Display Initialization ---
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("ESP32 MP3 Player");
  display.display();
  delay(1000);

  // --- SD Card Initialization ---
  display.println("Initializing SD card...");
  display.display();
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    display.println("SD Card Error!");
    display.display();
    while (1);
  }
  display.println("SD Card OK.");
  display.display();
  delay(1000);

  // --- Bluetooth Initialization ---
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Connecting to:");
  display.println(bt_address);
  display.display();

  a2dp_source.set_connection_state_cb(connection_state_changed);
  a2dp_source.start("Winamp");
  a2dp_source.connect(bt_address);

  // Initialize the MP3 decoder
  mp3dec_init(&mp3d);
}

// =======================================================================================
//   --- Main Application Loop ---
// =======================================================================================

void loop() {
  // If the 'is_playing' flag is true and there's data in the file, stream it.
  if (is_playing && mp3File.available()) {
    // Read a chunk of MP3 data from the SD card.
    int bytes_read = mp3File.read(mp3_buffer, 2048);
    // Decode the MP3 frame into PCM samples.
    int samples = mp3dec_decode_frame(&mp3d, mp3_buffer, bytes_read, pcm_buffer, &info);
    if (samples > 0) {
      // Write the decoded audio data to the Bluetooth sink.
      // The size must be in bytes, so we multiply by channels and sizeof(short).
      size_t bytes_written = a2dp_source.write_audio(pcm_buffer, samples * info.channels * sizeof(short));
    }
  } else if (is_playing && !mp3File.available()) {
    // If the file has ended, clean up and stop playing.
    is_playing = false;
    mp3File.close();
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Finished");
    display.display();
  }
}

// =======================================================================================
//   --- Callback Functions ---
// =======================================================================================

/**
 * @brief This callback is triggered by the A2DP library when the connection state changes.
 * @param state The new connection state.
 * @param object A pointer to the A2DP object (not used here).
 */
void connection_state_changed(esp_a2d_connection_state_t state, void *object) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    // When headphones connect, open the MP3 file and start playback.
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connected!");
    display.display();

    mp3File = SD.open(mp3FileName);
    if (!mp3File) {
      Serial.printf("Failed to open %s\n", mp3FileName);
      display.println("File Error!");
      display.display();
      while(1); // Halt on error
    }
    display.println("Playing...");
    display.display();
    is_playing = true; // Set the flag to start streaming in loop()

  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    // If headphones disconnect, stop playback and reboot the device to reconnect.
    is_playing = false;
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Disconnected");
    display.println("Rebooting...");
    display.display();
    delay(2000);
    ESP.restart();
  }
}
