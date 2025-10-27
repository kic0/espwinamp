#include <Arduino.h>
#include <SD.h>
#include <BluetoothA2DPSource.h>
#include <MP3DecoderHelix.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "BluetoothSerial.h"
#include <vector>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#include <esp_bt_defs.h>

// ---------- BT Discovery ----------
struct DiscoveredBTDevice {
    String name;
    esp_bd_addr_t address;
};
BluetoothSerial SerialBT;
std::vector<DiscoveredBTDevice> bt_devices;
int selected_bt_device = 0;
bool is_scanning = false;

// ---------- Playlist ----------
std::vector<String> playlists;
int selected_playlist = 0;
std::vector<String> current_playlist_files;
int current_song_index = 0;
bool is_playing = true;
bool song_started = false;

// ---------- Configuration ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ---------- Display ----------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Globals ----------
BluetoothA2DPSource a2dp;
libhelix::MP3DecoderHelix decoder;
File mp3File;
uint8_t read_buffer[1024];
int16_t pcm_buffer[4096];
int32_t pcm_buffer_len = 0;
int32_t pcm_buffer_offset = 0;

// Button states
bool scroll_pressed = false;
bool select_pressed = false;
unsigned long scroll_press_time = 0;
unsigned long select_press_time = 0;
const int long_press_duration = 1000; // 1 second
bool scroll_long_press_triggered = false;
bool select_long_press_triggered = false;

// App state
enum AppState {
  STARTUP,
  BT_DISCOVERY,
  SAMPLE_PLAYBACK,
  PLAYLIST_SELECTION,
  PLAYER
};
AppState currentState = STARTUP;

// forward declaration
int32_t get_sound_data(uint8_t *data, int32_t len);
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);


// ---------- Helper: Find MP3 ----------
String findFirstMP3() {
  File root = SD.open("/");
  if (!root) return String();

  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory() && strcasecmp(f.name() + strlen(f.name()) - 4, ".MP3") == 0) {
      String name = f.name();
      f.close();
      root.close();
      return name;
    }
    f = root.openNextFile();
  }
  root.close();
  return String();
}


// A2DP callback
int32_t get_sound_data(uint8_t *data, int32_t len) {
    if (pcm_buffer_len > 0){
        int32_t to_copy = pcm_buffer_len > len ? len : pcm_buffer_len;
        memcpy(data, (uint8_t*)pcm_buffer + pcm_buffer_offset, to_copy);
        pcm_buffer_len -= to_copy;
        pcm_buffer_offset += to_copy;
        return to_copy;
    }

    int bytes_read = mp3File.read(read_buffer, sizeof(read_buffer));
    if (bytes_read <= 0) {
        return 0; // stop if file is empty
    }

    decoder.write(read_buffer, bytes_read);

    // After writing to decoder, new data might be in pcm_buffer
    if (pcm_buffer_len > 0){
        int32_t to_copy = pcm_buffer_len > len ? len : pcm_buffer_len;
        memcpy(data, (uint8_t*)pcm_buffer + pcm_buffer_offset, to_copy);
        pcm_buffer_len -= to_copy;
        pcm_buffer_offset += to_copy;
        return to_copy;
    }

    return 0;
}


// pcm data callback
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    memcpy(pcm_buffer, pcm_buffer_cb, len);
    pcm_buffer_len = len;
    pcm_buffer_offset = 0;
}


void handle_button_press(bool is_short_press, bool is_scroll_button);
void handle_startup();
void handle_bt_discovery();
void handle_sample_playback();
void handle_playlist_selection();
void handle_player();
void play_song(String filename);


void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    // 0. Display init
    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(25, 25);
    display.println("Winamp");
    display.display();
    delay(2000); // Display splash for 2 seconds

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Starting...");
    display.display();

    // Buttons
    pinMode(BTN_SCROLL, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);

    // 1. SD init
    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] init failed!");
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("SD Card Error!");
        display.display();
        while (1);
    }
    Serial.println("[SD] ready");

    // Create /data directory if it doesn't exist
    if (!SD.exists("/data")) {
        if (SD.mkdir("/data")) {
            Serial.println("Created /data directory");
        } else {
            Serial.println("Failed to create /data directory");
        }
    }

    // BT Init
    SerialBT.begin("ESP32_Winamp", true);
    a2dp.start("ESP32_Winamp");
    Serial.println("Bluetooth device name set");
}


void loop() {
    // --- Button handling ---
    bool current_scroll = !digitalRead(BTN_SCROLL);
    bool current_select = !digitalRead(BTN_SELECT);

    // Scroll button
    if (current_scroll && !scroll_pressed) {
        scroll_pressed = true;
        scroll_press_time = millis();
        scroll_long_press_triggered = false;
    } else if (!current_scroll && scroll_pressed) {
        scroll_pressed = false;
        if (!scroll_long_press_triggered) {
            handle_button_press(true, true);
        }
    }
    if (scroll_pressed && !scroll_long_press_triggered && (millis() - scroll_press_time >= long_press_duration)) {
        handle_button_press(false, true);
        scroll_long_press_triggered = true;
    }

    // Select button
    if (current_select && !select_pressed) {
        select_pressed = true;
        select_press_time = millis();
        select_long_press_triggered = false;
    } else if (!current_select && select_pressed) {
        select_pressed = false;
        if (!select_long_press_triggered) {
            handle_button_press(true, false);
        }
    }
    if (select_pressed && !select_long_press_triggered && (millis() - select_press_time >= long_press_duration)) {
        handle_button_press(false, false);
        select_long_press_triggered = true;
    }

    // --- State machine ---
    switch (currentState) {
        case STARTUP:
            handle_startup();
            break;
        case BT_DISCOVERY:
            handle_bt_discovery();
            break;
        case SAMPLE_PLAYBACK:
            handle_sample_playback();
            break;
        case PLAYLIST_SELECTION:
            handle_playlist_selection();
            break;
        case PLAYER:
            handle_player();
            break;
    }
    delay(50);
}


void handle_button_press(bool is_short_press, bool is_scroll_button) {
    Serial.printf("Button press: short=%d, scroll=%d, state=%d\n", is_short_press, is_scroll_button, currentState);

    if (currentState == BT_DISCOVERY) {
        if (is_scroll_button && is_short_press) { // Scroll
            selected_bt_device++;
            if (selected_bt_device >= bt_devices.size()) {
                selected_bt_device = 0;
            }
        } else if (!is_scroll_button && is_short_press) { // Select
            if (!bt_devices.empty()) {
                DiscoveredBTDevice selected_device = bt_devices[selected_bt_device];
                Serial.printf("Selected device: %s\n", selected_device.name.c_str());

                // Stop scanning
                SerialBT.discoverAsyncStop();
                is_scanning = false;

                // Connect to the device
                if (a2dp.connect_to(selected_device.address)) {
                    Serial.println("Connected successfully!");

                    // Save the address to SD card
                    File file = SD.open("/data/bt_address.txt", FILE_WRITE);
                    if (file) {
                        char addr_str[18];
                        sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", selected_device.address[0], selected_device.address[1], selected_device.address[2], selected_device.address[3], selected_device.address[4], selected_device.address[5]);
                        file.print(addr_str);
                        file.close();
                        Serial.println("Saved BT address to SD card.");
                    } else {
                        Serial.println("Failed to save BT address.");
                    }

                    currentState = SAMPLE_PLAYBACK;
                } else {
                    Serial.println("Failed to connect.");
                    // Go back to scanning
                    is_scanning = false;
                }
            }
        }
    } else if (currentState == PLAYLIST_SELECTION) {
        if (is_scroll_button && is_short_press) { // Scroll
            selected_playlist++;
            if (selected_playlist >= playlists.size()) {
                selected_playlist = 0;
            }
        } else if (!is_scroll_button && is_short_press) { // Select
            if (!playlists.empty()) {
                String playlist_name = playlists[selected_playlist];
                Serial.printf("Selected playlist: %s\n", playlist_name.c_str());

                // Scan for mp3 files in the selected playlist folder
                current_playlist_files.clear();
                File playlist_folder = SD.open(playlist_name);
                File file = playlist_folder.openNextFile();
                while(file) {
                    if (!file.isDirectory() && String(file.name()).endsWith(".mp3")) {
                        current_playlist_files.push_back(playlist_name + "/" + String(file.name()));
                    }
                    file = playlist_folder.openNextFile();
                }
                playlist_folder.close();

                if (!current_playlist_files.empty()) {
                    current_song_index = 0;
                    currentState = PLAYER;
                } else {
                    Serial.println("No mp3 files found in this playlist!");
                }
            }
        }
    } else if (currentState == PLAYER) {
        if (is_scroll_button && is_short_press) { // Next song
            current_song_index++;
            if (current_song_index >= current_playlist_files.size()) {
                current_song_index = 0;
            }
            play_song(current_playlist_files[current_song_index]);
        } else if (is_scroll_button && !is_short_press) { // Previous song
            current_song_index--;
            if (current_song_index < 0) {
                current_song_index = current_playlist_files.size() - 1;
            }
            play_song(current_playlist_files[current_song_index]);
        } else if (!is_scroll_button && is_short_press) { // Play/Pause
            if (is_playing) {
                a2dp.set_data_callback(nullptr);
                is_playing = false;
                Serial.println("Paused");
            } else {
                a2dp.set_data_callback(get_sound_data);
                is_playing = true;
                Serial.println("Playing");
            }
        } else if (!is_scroll_button && !is_short_press) { // Back to playlist
            if (mp3File) mp3File.close();
            a2dp.set_data_callback(nullptr);
            decoder.end();
            song_started = false;
            currentState = PLAYLIST_SELECTION;
        }
    }
}


void handle_startup() {
    // Check for saved BT address and try to auto-connect
    File file = SD.open("/data/bt_address.txt", FILE_READ);
    if (file) {
        String addr_str = file.readString();
        file.close();
        Serial.printf("Found saved BT address: %s\n", addr_str.c_str());

        esp_bd_addr_t addr;
        sscanf(addr_str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);

        if (a2dp.connect_to(addr)) {
            Serial.println("Auto-connected successfully!");
            currentState = SAMPLE_PLAYBACK;
            return;
        } else {
            Serial.println("Auto-connect failed.");
        }
    }

    // If no saved address or auto-connect fails, go to discovery
    currentState = BT_DISCOVERY;
}

void draw_bt_discovery_ui();

void handle_bt_discovery() {
    if (!is_scanning) {
        Serial.println("Starting BT device discovery...");
        bt_devices.clear();
        SerialBT.discoverAsync([](BTAdvertisedDevice* pDevice) {
            if (pDevice->haveName()) {
                // To avoid duplicates
                bool found = false;
                for (const auto& dev : bt_devices) {
                    if (memcmp(dev.address, pDevice->getAddress().getNative(), ESP_BD_ADDR_LEN) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    DiscoveredBTDevice new_device;
                    new_device.name = pDevice->getName().c_str();
                    memcpy(new_device.address, pDevice->getAddress().getNative(), ESP_BD_ADDR_LEN);
                    bt_devices.push_back(new_device);
                }
            }
        });
        is_scanning = true;
    }

    draw_bt_discovery_ui();
}

void handle_sample_playback() {
    static bool sample_started = false;
    if (!sample_started) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Playing connection sound...");
        display.display();
        a2dp.set_volume(32); // 25% volume
        play_song("/data/sample.mp3");
        sample_started = true;
    }

    if (mp3File.available() <= 0) {
        sample_started = false;
        a2dp.set_volume(127); // 100% volume
        currentState = PLAYLIST_SELECTION;
    }
}

void draw_bt_discovery_ui() {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Select BT Speaker:");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (bt_devices.empty()) {
        display.setCursor(0, 15);
        display.println("Scanning...");
    } else {
        for (int i = 0; i < bt_devices.size(); i++) {
            if (i == selected_bt_device) {
                display.print("> ");
            } else {
                display.print("  ");
            }
            display.println(bt_devices[i].name.c_str());
        }
    }
    display.display();
}

void find_playlists() {
    playlists.clear();
    File root = SD.open("/");
    if (!root) return;

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory() && strcmp(file.name(), "/data") != 0) {
            playlists.push_back(file.name());
        }
        file = root.openNextFile();
    }
    root.close();
}

void draw_playlist_ui() {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Select Playlist:");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (playlists.empty()) {
        display.setCursor(0, 15);
        display.println("No playlists found!");
    } else {
        for (int i = 0; i < playlists.size(); i++) {
            if (i == selected_playlist) {
                display.print("> ");
            } else {
                display.print("  ");
            }
            display.println(playlists[i].c_str());
        }
    }
    display.display();
}

void handle_playlist_selection() {
    if (playlists.empty()) {
        find_playlists();
    }
    draw_playlist_ui();
}

void draw_player_ui() {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Now Playing:");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 15);
    if (!current_playlist_files.empty()) {
        display.println(current_playlist_files[current_song_index].c_str());
    }
    display.display();
}

void play_song(String filename) {
    if (mp3File) {
        mp3File.close();
    }
    mp3File = SD.open(filename);
    if (!mp3File) {
        Serial.println("Failed to open song file!");
        return;
    }
    decoder.setDataCallback(pcm_data_callback);
    a2dp.set_data_callback(get_sound_data);
    is_playing = true;
    Serial.println("Playing song");
}

void handle_player() {
    if (!song_started) {
        play_song(current_playlist_files[current_song_index]);
        song_started = true;
    }

    if (mp3File && mp3File.available() <= 0) {
        current_song_index++;
        if (current_song_index >= current_playlist_files.size()) {
            current_song_index = 0;
        }
        play_song(current_playlist_files[current_song_index]);
    }

    draw_player_ui();
}
