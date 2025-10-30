#include <Arduino.h>
#include <SD.h>
#include <BluetoothA2DPSource.h>
#include <MP3DecoderHelix.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "esp_gap_bt_api.h"
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
std::vector<DiscoveredBTDevice> bt_devices;
int selected_bt_device = 0;
bool is_scanning = false;
bool is_bt_connected = false;
bool initial_scan_complete = false;
unsigned long connection_start_time = 0;

// ---------- Playlist ----------
std::vector<String> playlists;
int selected_playlist = 0;
std::vector<String> current_playlist_files;
int current_song_index = 0;
bool is_playing = true;
bool song_started = false;
bool sample_started = false;

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
  BT_CONNECTING,
  SAMPLE_PLAYBACK,
  PLAYLIST_SELECTION,
  PLAYER
};
AppState currentState = STARTUP;

// forward declaration
int32_t get_data_frames(Frame *frame, int32_t frame_count);
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);
void bt_connection_state_cb(esp_a2d_connection_state_t state, void* ptr);


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
int32_t get_data_frames(Frame *frame, int32_t frame_count) {
    // If we don't have enough PCM data, read from file and decode
    if (pcm_buffer_len < frame_count) {
        if (mp3File && mp3File.available()) {
            int bytes_read = mp3File.read(read_buffer, sizeof(read_buffer));
            if (bytes_read > 0) {
                decoder.write(read_buffer, bytes_read);
            }
        } else {
            // End of file or file not open
            return 0;
        }
    }

    // Determine how many frames we can actually provide
    int32_t frames_to_provide = (pcm_buffer_len < frame_count) ? pcm_buffer_len : frame_count;

    if (frames_to_provide > 0) {
        // Copy PCM data into the frame structure
        for (int i = 0; i < frames_to_provide; i++) {
            frame[i].channel1 = pcm_buffer[pcm_buffer_offset + i];
            frame[i].channel2 = pcm_buffer[pcm_buffer_offset + i];
        }

        // Update buffer offsets
        pcm_buffer_len -= frames_to_provide;
        pcm_buffer_offset += frames_to_provide;

        // Reset offset if buffer is empty
        if (pcm_buffer_len == 0) {
            pcm_buffer_offset = 0;
        }
    }

    return frames_to_provide;
}


// pcm data callback
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    memcpy(pcm_buffer, pcm_buffer_cb, len * sizeof(int16_t));
    pcm_buffer_len = len;
    pcm_buffer_offset = 0;
}


void handle_button_press(bool is_short_press, bool is_scroll_button);
void handle_startup();
void handle_bt_discovery();
void handle_bt_connecting();
void handle_sample_playback();
void handle_playlist_selection();
void handle_player();
void play_song(String filename);

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
void get_bt_device_props(esp_bt_gap_cb_param_t *param);


void setup() {
    // a 2-second delay at the very beginning, before any other code is executed
    delay(2000); // Wait for power to stabilize
    Serial.begin(115200);
    while (!Serial) delay(10);

    // Buttons
    pinMode(BTN_SCROLL, INPUT_PULLUP);

    // 1. SD init
    Serial.println("Initializing SD Card...");
    bool sd_ok = false;
    for (int i = 0; i < 5; i++) {
        if (SD.begin(SD_CS)) {
            sd_ok = true;
            break;
        }
        Serial.printf("SD Card init attempt %d failed\n", i + 1);
        delay(500);
    }

    if (!sd_ok) {
        Serial.println("[SD] init failed after multiple attempts! Restarting...");
        delay(1000); // delay to allow serial message to be sent
        ESP.restart();
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

    // Delay before BT
    delay(4000);

    // 2. BT Init
    Serial.println("Starting A2DP source...");
    a2dp.set_on_connection_state_changed(bt_connection_state_cb);
    a2dp.start("winamp");
    Serial.println("A2DP started, device name set to winamp");

    esp_err_t err;
    if ((err = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
        Serial.printf("esp_bt_gap_register_callback() FAILED: %s\n", esp_err_to_name(err));
        return;
    }

    // 3. Decoder init
    decoder.begin();
    decoder.setDataCallback(pcm_data_callback);

    // Delay before Display
    delay(3000);

    // 4. Display init
    Serial.println("Initializing Display...");
    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Halt if display fails, as it's critical for UI
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(25, 25);
    display.println("Winamp");
    display.display();
    delay(2000); // Display splash
}


void loop() {
    // --- Button handling ---
    bool current_scroll = !digitalRead(BTN_SCROLL);

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

    // --- State machine ---
    switch (currentState) {
        case STARTUP:
            handle_startup();
            break;
        case BT_DISCOVERY:
            handle_bt_discovery();
            break;
        case BT_CONNECTING:
            handle_bt_connecting();
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
        if (is_scroll_button && is_short_press) { // Scroll with short press
            selected_bt_device++;
            if (selected_bt_device >= bt_devices.size()) {
                selected_bt_device = 0;
            }
        } else if (is_scroll_button && !is_short_press) { // Select with long press
            if (!bt_devices.empty()) {
                DiscoveredBTDevice selected_device = bt_devices[selected_bt_device];
                Serial.printf("Selected device: %s\n", selected_device.name.c_str());

                // Stop scanning
                esp_bt_gap_cancel_discovery();
                is_scanning = false;

                // Connect to the device
                if (a2dp.connect_to(selected_device.address)) {
                    connection_start_time = millis();
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

                    currentState = BT_CONNECTING;
                } else {
                    Serial.println("Failed to connect.");
                    // Go back to scanning
                    is_scanning = false;
                }
            }
        }
    } else if (currentState == PLAYLIST_SELECTION) {
        if (is_scroll_button && is_short_press) { // Scroll with short press
            selected_playlist++;
            if (selected_playlist >= playlists.size()) {
                selected_playlist = 0;
            }
        } else if (is_scroll_button && !is_short_press) { // Select with long press
            if (!playlists.empty()) {
                String playlist_name = "/" + playlists[selected_playlist];
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
                a2dp.set_data_callback_in_frames(get_data_frames);
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
    // Always start with BT discovery
    currentState = BT_DISCOVERY;
}

void draw_bt_discovery_ui();

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
void get_bt_device_props(esp_bt_gap_cb_param_t *param);
void attempt_auto_connect();

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            get_bt_device_props(param);
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                if (currentState == BT_DISCOVERY) {
                    is_scanning = false;
                    initial_scan_complete = true;
                    Serial.println("BT discovery stopped.");
                    attempt_auto_connect();
                }
            }
            break;
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
                Serial.printf("Remote name response for %02x:%02x:%02x:%02x:%02x:%02x\n", param->read_rmt_name.bda[0], param->read_rmt_name.bda[1], param->read_rmt_name.bda[2], param->read_rmt_name.bda[3], param->read_rmt_name.bda[4], param->read_rmt_name.bda[5]);
                for (auto& device : bt_devices) {
                    if (memcmp(device.address, param->read_rmt_name.bda, ESP_BD_ADDR_LEN) == 0) {
                        device.name = String((char*)param->read_rmt_name.rmt_name);
                        Serial.printf("  Name updated to: %s\n", device.name.c_str());
                        break;
                    }
                }
            } else {
                Serial.println("Failed to read remote name.");
            }
            break;
        default:
            break;
    }
}

void get_bt_device_props(esp_bt_gap_cb_param_t *param) {
    char bda_str[18];
    sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
            param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
    Serial.printf("Device found: %s\n", bda_str);

    // Check if device is already in the list
    for (const auto& dev : bt_devices) {
        if (memcmp(dev.address, param->disc_res.bda, ESP_BD_ADDR_LEN) == 0) {
            return; // Already found, do nothing
        }
    }

    DiscoveredBTDevice new_device;
    memcpy(new_device.address, param->disc_res.bda, ESP_BD_ADDR_LEN);

    char *name = NULL;
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
            name = (char *)param->disc_res.prop[i].val;
            new_device.name = String(name);
            Serial.printf("  Name: %s\n", new_device.name.c_str());
            break;
        }
    }

    // If no name was found, use the MAC address as the name for now
    // and request the remote name
    if (name == NULL) {
        new_device.name = String(bda_str);
        Serial.println("  Name: Not found, requesting remote name.");
        esp_bt_gap_read_remote_name(param->disc_res.bda);
    }

    // Add the new device to the list
    bt_devices.push_back(new_device);
}

void attempt_auto_connect() {
    File file = SD.open("/data/bt_address.txt", FILE_READ);
    if (!file) {
        Serial.println("No saved BT address found.");
        return;
    }

    String addr_str = file.readString();
    file.close();
    if (addr_str.length() == 0) {
        return;
    }

    esp_bd_addr_t saved_addr;
    sscanf(addr_str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &saved_addr[0], &saved_addr[1], &saved_addr[2], &saved_addr[3], &saved_addr[4], &saved_addr[5]);

    // Check if the saved device is in our discovered list
    for (const auto& device : bt_devices) {
        if (memcmp(device.address, saved_addr, ESP_BD_ADDR_LEN) == 0) {
            Serial.printf("Saved device %s found in scan results. Attempting to connect...\n", addr_str.c_str());
            connection_start_time = millis();
            a2dp.connect_to(saved_addr);
            currentState = BT_CONNECTING;
            return;
        }
    }

    Serial.println("Saved device not found in scan results.");
}

void handle_bt_discovery() {
    if (!is_scanning && !initial_scan_complete) {
        Serial.println("Starting BT device discovery...");
        bt_devices.clear();

        // Use the enum defined in esp_gap_bt_api.h
        esp_bt_inq_mode_t mode = ESP_BT_INQ_MODE_GENERAL_INQUIRY; // 0
        uint8_t inquiry_length = 10;   // seconds
        uint8_t num_responses = 0;     // let it return all

        esp_bt_gap_start_discovery(mode, inquiry_length, num_responses);
        is_scanning = true;
    }
    draw_bt_discovery_ui();
}

void handle_bt_connecting() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Connecting...");
    display.display();

    if (is_bt_connected) {
        Serial.println("Connection established. Waiting 2 seconds for stability...");
        a2dp.set_volume(64); // Set volume to 50%
        delay(2000);
        currentState = SAMPLE_PLAYBACK;
    } else if (millis() - connection_start_time > 15000) { // 15 second timeout
        Serial.println("Connection timeout. Returning to discovery.");
        a2dp.disconnect();
        is_bt_connected = false;
        initial_scan_complete = false; // Allow rescanning
        currentState = BT_DISCOVERY;
    }
}

void handle_sample_playback() {
    if (!is_bt_connected) {
        Serial.println("BT disconnected during sample playback. Returning to discovery.");
        if (mp3File) mp3File.close();
        decoder.end();
        sample_started = false;
        is_playing = false;
        initial_scan_complete = false; // Allow rescanning
        currentState = BT_DISCOVERY;
        return;
    }

    if (!sample_started) {
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(25, 25);
        display.println("Winamp");
        display.display();
        a2dp.set_volume(127); // 100% volume
        play_song("/data/sample.mp3");
        sample_started = true;
    }

    // The audio data is now handled by the a2dp_data_callback.
    // We just need to check if the file has finished.
    if (!mp3File || !mp3File.available()) {
        Serial.println("Sample finished or file not available.");
        sample_started = false;
        currentState = PLAYLIST_SELECTION;
    }
}

void draw_bt_discovery_ui() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Select BT Speaker:");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (bt_devices.empty()) {
        display.setCursor(0, 15 + 5);
        display.println("Scanning...");
    } else {
        display.setCursor(0, 15 + 5);
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
        if (file.isDirectory() && strcmp(file.name(), "data") != 0 && strcmp(file.name(), "System Volume Information") != 0) {
            playlists.push_back(file.name());
        }
        file = root.openNextFile();
    }
    root.close();
}

void draw_playlist_ui() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Select Playlist:");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (playlists.empty()) {
        display.setCursor(0, 15 + 5);
        display.println("No playlists found!");
    } else {
        display.setCursor(0, 15 + 5);
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
    display.setCursor(0, 15 + 5);
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
    a2dp.set_data_callback_in_frames(get_data_frames);
    is_playing = true;
    song_started = true;
    Serial.println("Playing song");
}

void handle_player() {
    if (!is_bt_connected) {
        Serial.println("BT disconnected during playback. Returning to discovery.");
        if (mp3File) mp3File.close();
        decoder.end();
        song_started = false;
        is_playing = false;
        initial_scan_complete = false; // Allow rescanning
        currentState = BT_DISCOVERY;
        return;
    }

    if (!song_started) {
        play_song(current_playlist_files[current_song_index]);
    }

    // The audio data is now handled by the a2dp_data_callback.
    // We just need to check if the file has finished and play the next one.
    if (is_playing && mp3File && !mp3File.available()) {
        Serial.println("Song finished, playing next.");
        current_song_index++;
        if (current_song_index >= current_playlist_files.size()) {
            current_song_index = 0;
        }
        play_song(current_playlist_files[current_song_index]);
    }

    draw_player_ui();
}

void bt_connection_state_cb(esp_a2d_connection_state_t state, void* ptr){
    Serial.printf("A2DP connection state changed: %d\n", state);
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        is_bt_connected = true;
    } else {
        is_bt_connected = false;
    }
}