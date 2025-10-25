/*
  ESP32 MP3 Player for LilyGo T-Display S3
  - Streams MP3 files from an SD card to Bluetooth headphones.
  - Features a UI for device discovery and playback control.
  -
  - HARDWARE:
  -  - LilyGo T-Display S3
  -  - SD Card Reader connected via SPI
  -
  - WIRING:
  -  - SD_CS:    GPIO 10
  -  - SPI_MOSI: GPIO 11
  -  - SPI_MISO: GPIO 13
  -  - SPI_SCLK: GPIO 12
*/

// Display Libraries
#include <TFT_eSPI.h>
#include <SPI.h>

// SD Card Library
#include "SD.h"

// Bluetooth A2DP Library
#include "BluetoothA2DPSource.h"

// MP3 Decoder Library
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

// Pin Definitions for LilyGo T-Display S3
#define BUTTON1_PIN 0
#define BUTTON2_PIN 14
#define SD_CS_PIN   10

// --- Global Objects ---
TFT_eSPI tft = TFT_eSPI();
BluetoothA2DPSource a2dp_source;

// --- Global Variables ---
enum AppState {
  STATE_SCANNING,
  STATE_SELECTING_DEVICE,
  STATE_CONNECTING,
  STATE_PLAYING,
  STATE_PAUSED
};

AppState currentState = STATE_SCANNING;
std::vector<BTAddress> discoveredDevices;
int selectedDeviceIndex = 0;

// MP3 File
File mp3File;
char mp3FileName[100] = "/data/sample.mp3";

// MP3 Decoder
mp3dec_t mp3d;
mp3dec_frame_info_t info;
unsigned char mp3_buffer[2048];
short pcm_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];


void setup() {
  // --- Serial Initialization ---
  Serial.begin(115200);

  // --- Display Initialization ---
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.println("Starting up...");

  // --- Button Initialization ---
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  // --- SD Card Initialization ---
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Card Mount Failed");
    tft.fillScreen(TFT_RED);
    tft.drawString("SD Card Error!", 20, 20);
    while (1); // Halt on error
  }
  Serial.println("SD Card initialized.");
  tft.println("SD Card OK.");
  delay(1000);

  // --- Bluetooth Initialization ---
  a2dp_source.set_stack_event_callback(bt_av_hdl_stack_evt);
  a2dp_source.set_connection_state_cb(bt_app_connection_status_cb);
  a2dp_source.set_audio_data_cb(bt_app_a2d_data_cb);

  a2dp_source.start("Winamp");
  a2dp_source.set_discoverability(ESP_BT_GENERAL_DISCOVERABLE);

  // Start the discovery process
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Scanning...", 20, 20);
  a2dp_source.discover_devices();
}

/**
 * @brief This function is called by the A2DP library when a Bluetooth device is discovered.
 * It adds the discovered device to our list.
 */
void bt_av_hdl_stack_evt(uint16_t event, void *p_param) {
  if (event == ESP_A2D_SOURCE_DISCOVERY_RESULT_EVT) {
    esp_a2d_disc_evt_param_t *param = (esp_a2d_disc_evt_param_t *)p_param;
    if (param->result.status == ESP_A2D_DISC_SUCCESS) {
      discoveredDevices.push_back(BTAddress(param->result.bda));
      Serial.printf("Device found: %s\n", discoveredDevices.back().toString().c_str());
    }
  }
}

/**
 * @brief Renders the list of discovered Bluetooth devices on the T-Display.
 * The currently selected device is highlighted.
 */
void drawDeviceList() {
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Select a device:", 0, 0);
  for (int i = 0; i < discoveredDevices.size(); i++) {
    if (i == selectedDeviceIndex) {
      tft.setTextColor(TFT_BLACK, TFT_WHITE); // Highlight selected
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tft.drawString(discoveredDevices[i].toString().c_str(), 5, 20 + (i * 15));
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Reset color
}

// =======================================================================================
//   --- Main Application Loop ---
// =======================================================================================

void loop() {
  // Main state machine
  switch (currentState) {
    // --- STATE: Scanning for Bluetooth Devices ---
    case STATE_SCANNING:
      // When discovery is finished, move to the selection state
      if (!a2dp_source.is_device_discovery_running()) {
        currentState = STATE_SELECTING_DEVICE;
        drawDeviceList();
      }
      break;

    // --- STATE: User is Selecting a Device ---
    case STATE_SELECTING_DEVICE:
      // Button 2: Scroll through the list
      if (digitalRead(BUTTON2_PIN) == LOW) {
        selectedDeviceIndex++;
        if (selectedDeviceIndex >= discoveredDevices.size()) {
          selectedDeviceIndex = 0;
        }
        drawDeviceList();
        delay(200); // Simple debounce
      }

      // Button 1: Confirm selection and start connecting
      if (digitalRead(BUTTON1_PIN) == LOW) {
        currentState = STATE_CONNECTING;
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Connecting to:", 0, 0);
        tft.drawString(discoveredDevices[selectedDeviceIndex].toString().c_str(), 5, 20);
        a2dp_source.connect_to(discoveredDevices[selectedDeviceIndex]);
        delay(200); // Simple debounce
      }
      break;

    // --- STATE: Connecting to the Selected Device ---
    case STATE_CONNECTING:
      // In this state, we just wait for the connection callback to trigger.
      // A timeout could be added here for better error handling.
      break;

    // --- STATE: Music is Playing ---
    case STATE_PLAYING:
      // Main audio pipeline: read -> decode -> stream
      if (mp3File.available()) {
        int bytes_read = mp3File.read(mp3_buffer, 2048);
        int samples = mp3dec_decode_frame(&mp3d, mp3_buffer, bytes_read, pcm_buffer, &info);
        if (samples > 0) {
          // Write the decoded PCM data to the Bluetooth sink
          size_t bytes_written = a2dp_source.write_audio(pcm_buffer, samples * info.channels * sizeof(short));
        }
      } else {
        // If the file ends, go to paused state
        currentState = STATE_PAUSED;
        tft.drawString("Paused", 5, 40);
      }

      // Button 1: Pause the music
      if (digitalRead(BUTTON1_PIN) == LOW) {
        currentState = STATE_PAUSED;
        tft.drawString("Paused", 5, 40);
        delay(200); // Debounce
      }
      break;

    // --- STATE: Music is Paused ---
    case STATE_PAUSED:
      // Button 1: Resume playing
      if (digitalRead(BUTTON1_PIN) == LOW) {
        currentState = STATE_PLAYING;
        // Redraw the screen to clear the "Paused" message
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Playing:", 0, 0);
        tft.drawString(mp3FileName, 5, 20);
        delay(200); // Debounce
      }

      // Button 2: Skip to the beginning of the song
      if (digitalRead(BUTTON2_PIN) == LOW) {
        mp3File.seek(0); // Reset file position to the start
        currentState = STATE_PLAYING;
        delay(200); // Debounce
      }
      break;

    default:
      // Should not happen
      break;
  }
}


// =======================================================================================
//   --- Callback Functions ---
// =======================================================================================

/**
 * @brief This is an optional callback for the A2DP library. In source mode,
 * the main loop drives the data, so this isn't strictly necessary.
 */
int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len) {
    return len;
}

/**
 * @brief Opens the specified MP3 file and prepares the display for playback.
 * Halts on error if the file cannot be opened.
 */
void startPlayingFile() {
  mp3File = SD.open(mp3FileName);
  if (!mp3File) {
    Serial.printf("Failed to open %s\n", mp3FileName);
    tft.fillScreen(TFT_RED);
    tft.drawString("File Error!", 20, 20);
    while (1); // Halt execution
  }

  tft.fillScreen(TFT_BLACK);
  tft.drawString("Playing:", 0, 0);
  tft.drawString(mp3FileName, 5, 20);
}

/**
 * @brief This function is called by the A2DP library when the connection state changes.
 * It's used to transition to the PLAYING state upon successful connection.
 */
void bt_app_connection_status_cb(esp_a2d_connection_state_t new_state, void *object) {
  if (new_state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    currentState = STATE_PLAYING;
    Serial.println("Connected!");
    startPlayingFile();
  } else if (new_state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    // Here you could add logic to return to the scanning state, for example.
    currentState = STATE_SCANNING;
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Disconnected", 0, 0);
    delay(2000);
    // Restart discovery
    a2dp_source.discover_devices();
  }
}
