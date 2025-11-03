#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
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
int bt_discovery_scroll_offset = 0;
bool is_scanning = false;
bool is_bt_connected = false;
bool initial_scan_complete = false;
unsigned long connection_start_time = 0;

// ---------- Artists ----------
std::vector<String> artists;
int selected_artist = 0;
int artist_scroll_offset = 0;

// ---------- Playlist ----------
std::vector<String> playlists;
int selected_playlist = 0;
int playlist_scroll_offset = 0;
std::vector<String> current_playlist_files;
int current_song_index = 0;
int selected_song_in_player = 0;
int player_scroll_offset = 0;
bool is_playing = true;
bool song_started = false;
bool sample_started = false;
bool ui_dirty = true;

// ---------- Marquee ----------
const int MAX_MARQUEE_LINES = 6;
bool is_marquee_active[MAX_MARQUEE_LINES] = {false};
unsigned long marquee_start_time[MAX_MARQUEE_LINES] = {0};
String marquee_text[MAX_MARQUEE_LINES];

// ---------- Configuration ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ---------- Display ----------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Globals ----------
volatile int diag_sample_rate = 0;
volatile int diag_bits_per_sample = 0;
volatile int diag_channels = 0;

BluetoothA2DPSource a2dp;
libhelix::MP3DecoderHelix decoder;
File mp3File;
uint8_t read_buffer[1024];
int16_t pcm_buffer[4096];
int32_t pcm_buffer_len = 0;

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
  ARTIST_SELECTION,
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
    if (pcm_buffer_len == 0) {
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
    int32_t frames_to_provide = (pcm_buffer_len / 2 < frame_count) ? pcm_buffer_len / 2 : frame_count;

    if (frames_to_provide > 0) {
        // Copy PCM data into the frame structure, de-interleaving stereo
        for (int i = 0; i < frames_to_provide; i++) {
            frame[i].channel1 = pcm_buffer[i * 2];
            frame[i].channel2 = pcm_buffer[i * 2 + 1];
        }

        // Shift the remaining data to the beginning of the buffer
        int samples_consumed = frames_to_provide * 2;
        pcm_buffer_len -= samples_consumed;
        if (pcm_buffer_len > 0) {
            memmove(pcm_buffer, pcm_buffer + samples_consumed, pcm_buffer_len * sizeof(int16_t));
        }
    }

    return frames_to_provide;
}


// pcm data callback
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    // Safely store diagnostic info
    diag_sample_rate = info.samprate;
    diag_bits_per_sample = info.bitsPerSample;
    diag_channels = info.nChans;

    // Append new PCM data to the buffer
    if (pcm_buffer_len + len < sizeof(pcm_buffer) / sizeof(int16_t)) {
        memcpy(pcm_buffer + pcm_buffer_len, pcm_buffer_cb, len * sizeof(int16_t));
        pcm_buffer_len += len;
    } else {
        // Buffer overflow, handle error (e.g., log it)
        Serial.println("PCM buffer overflow!");
    }
}

void draw_dynamic_text(String text, int y, int x_offset, bool allow_scroll, int line_index) {
    if (line_index >= MAX_MARQUEE_LINES) return;

    int16_t x_b, y_b;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x_b, &y_b, &w, &h);

    int max_width = SCREEN_WIDTH - x_offset;

    if (w <= max_width) {
        display.setCursor(x_offset, y);
        display.print(text.c_str());
        is_marquee_active[line_index] = false;
    } else if (allow_scroll) {
        if (!is_marquee_active[line_index] || marquee_text[line_index] != text) {
            is_marquee_active[line_index] = true;
            marquee_start_time[line_index] = millis();
            marquee_text[line_index] = text;
        }

        int text_width = w;
        int scroll_distance = text_width - max_width;
        unsigned long scroll_duration = text_width * 20;
        unsigned long time_since_start = millis() - marquee_start_time[line_index];
        int current_x_offset = (time_since_start % scroll_duration) * scroll_distance / scroll_duration;

        display.setCursor(x_offset - current_x_offset, y);
        display.print(text.c_str());
    } else { // Truncate
        String truncated_text = text;
        display.getTextBounds(truncated_text + "...", 0, 0, &x_b, &y_b, &w, &h);
        while (w > max_width) {
            truncated_text = truncated_text.substring(0, truncated_text.length() - 1);
            display.getTextBounds(truncated_text + "...", 0, 0, &x_b, &y_b, &w, &h);
        }
        display.setCursor(x_offset, y);
        display.print(truncated_text + "...");
        is_marquee_active[line_index] = false;
    }
}

void handle_button_press(bool is_short_press, bool is_scroll_button);
void handle_startup();
void handle_bt_discovery();
void handle_bt_connecting();
void handle_sample_playback();
void handle_artist_selection();
void draw_artist_ui();
void handle_playlist_selection();
void draw_playlist_ui();
void handle_player();
void draw_player_ui();
void play_file(String filename, bool from_spiffs);
void play_song(String filename);

void calculate_scroll_offset(int &selected_item, int item_count, int &scroll_offset, int center_offset) {
    if (selected_item >= item_count) {
        selected_item = 0;
    } else if (selected_item < 0) {
        selected_item = item_count - 1;
    }

    if (item_count <= 4) {
        scroll_offset = 0;
        return;
    }

    if (selected_item < scroll_offset + center_offset) {
        scroll_offset = selected_item - center_offset;
        if (scroll_offset < 0) {
            scroll_offset = 0;
        }
    } else if (selected_item >= scroll_offset + 4 - center_offset) {
        scroll_offset = selected_item - (3 - center_offset);
        if (scroll_offset > item_count - 4) {
            scroll_offset = item_count - 4;
        }
    }
}

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

    // 1. SPIFFS init
    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

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
    display.setTextWrap(false);
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(25, 25);
    display.println("Winamp");
    display.display();
    delay(2000); // Display splash
}


void loop() {
    static unsigned long last_heap_log = 0;
    if (millis() - last_heap_log > 2000) {
        Serial.printf("Free heap: %d bytes | Decoder: sample_rate=%d, bps=%d, channels=%d\n",
                      ESP.getFreeHeap(), diag_sample_rate, diag_bits_per_sample, diag_channels);
        last_heap_log = millis();
    }
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
    bool should_refresh_for_marquee = false;
    for (int i = 0; i < MAX_MARQUEE_LINES; i++) {
        if (is_marquee_active[i]) {
            should_refresh_for_marquee = true;
            break;
        }
    }

    if (should_refresh_for_marquee) {
        ui_dirty = true;
    }

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
        case ARTIST_SELECTION:
            handle_artist_selection();
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
            // Simple viewport scrolling
            if (selected_bt_device >= bt_discovery_scroll_offset + 4) {
                bt_discovery_scroll_offset = selected_bt_device - 3;
            }
            if (selected_bt_device < bt_discovery_scroll_offset) {
                bt_discovery_scroll_offset = selected_bt_device;
            }
            ui_dirty = true;
        } else if (is_scroll_button && !is_short_press) { // Select with long press
            if (!bt_devices.empty()) {
                DiscoveredBTDevice selected_device = bt_devices[selected_bt_device];
                Serial.printf("Selected device: %s\n", selected_device.name.c_str());

                // Allow a moment for any pending remote name requests to complete
                delay(1000);

                // Stop scanning
                esp_bt_gap_cancel_discovery();
                is_scanning = false;

                // Connect to the device
                if (a2dp.connect_to(selected_device.address)) {
                    connection_start_time = millis();
                    // Save the address to SPIFFS
                    File file = SPIFFS.open("/bt_address.txt", FILE_WRITE);
                    if (file) {
                        char addr_str[18];
                        sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", selected_device.address[0], selected_device.address[1], selected_device.address[2], selected_device.address[3], selected_device.address[4], selected_device.address[5]);
                        file.print(addr_str);
                        file.close();
                        Serial.println("Saved BT address to SPIFFS.");
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
    } else if (currentState == ARTIST_SELECTION) {
        if (is_scroll_button && is_short_press) { // Scroll with short press
            selected_artist++;
            calculate_scroll_offset(selected_artist, artists.size(), artist_scroll_offset, 2);
            for (int i=0; i<MAX_MARQUEE_LINES; ++i) is_marquee_active[i] = false;
            ui_dirty = true;
        } else if (is_scroll_button && !is_short_press) { // Select with long press
            if (!artists.empty()) {
                // Clear playlist data from any previous artist selection
                playlists.clear();
                selected_playlist = 0;
                playlist_scroll_offset = 0;

                // Transition to playlist selection for the chosen artist
                currentState = PLAYLIST_SELECTION;
                ui_dirty = true;
            }
        }
    } else if (currentState == PLAYLIST_SELECTION) {
        if (is_scroll_button && is_short_press) { // Scroll with short press
            selected_playlist++;
            calculate_scroll_offset(selected_playlist, playlists.size() + 1, playlist_scroll_offset, 2);
            for (int i=0; i<MAX_MARQUEE_LINES; ++i) is_marquee_active[i] = false;
            ui_dirty = true;
        } else if (is_scroll_button && !is_short_press) { // Select with long press
            if (selected_playlist == playlists.size()) {
                // This is the "back" button
                currentState = ARTIST_SELECTION;
                ui_dirty = true;
            } else if (!playlists.empty()) {
                String artist_name = artists[selected_artist];
                String playlist_name = playlists[selected_playlist];
                String full_path = "/" + artist_name + "/" + playlist_name;
                Serial.printf("Selected playlist: %s\n", full_path.c_str());

                // Scan for mp3 files in the selected playlist folder
                current_playlist_files.clear();
                File playlist_folder = SD.open(full_path);
                File file = playlist_folder.openNextFile();
                while(file) {
                    if (!file.isDirectory() && String(file.name()).endsWith(".mp3")) {
                        current_playlist_files.push_back(full_path + "/" + String(file.name()));
                    }
                    file = playlist_folder.openNextFile();
                }
                playlist_folder.close();

                if (!current_playlist_files.empty()) {
                    current_song_index = 0;
                    selected_song_in_player = 0;
                    player_scroll_offset = 0;
                    song_started = false;
                    ui_dirty = true;
                    currentState = PLAYER;
                } else {
                    Serial.println("No mp3 files found in this playlist!");
                }
            }
        }
    } else if (currentState == PLAYER) {
        if (is_scroll_button && is_short_press) { // Scroll through songs
            selected_song_in_player++;
            calculate_scroll_offset(selected_song_in_player, current_playlist_files.size() + 1, player_scroll_offset, 2);
            for (int i=0; i<MAX_MARQUEE_LINES; ++i) is_marquee_active[i] = false;
            ui_dirty = true;
        } else if (is_scroll_button && !is_short_press) { // Select and play a song
            if (selected_song_in_player == current_playlist_files.size()) {
                // This is the "back" button
                currentState = PLAYLIST_SELECTION;
                ui_dirty = true;
            } else if (current_song_index != selected_song_in_player || !song_started) {
                current_song_index = selected_song_in_player;
                play_song(current_playlist_files[current_song_index]);
            }
        }
    }
}


void handle_startup() {
    // Always start with BT discovery
    bt_discovery_scroll_offset = 0;
    ui_dirty = true;
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
                        ui_dirty = true;
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
    ui_dirty = true;
}

void attempt_auto_connect() {
    File file = SPIFFS.open("/bt_address.txt", FILE_READ);
    if (!file) {
        Serial.println("No saved BT address found.");
        return;
    }

    String addr_str = file.readString();
    file.close();
    if (addr_str.length() != 17) {
        Serial.printf("Invalid BT address length in SPIFFS: %d\n", addr_str.length());
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
        Serial.println("Connection established.");
        a2dp.set_volume(64); // Set volume to 50%
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
    // Static variables are used here to maintain state across loop() calls.
    // They are initialized once when the function is first called and retain their values.
    // splash_start_time is reset to 0 when this state is exited, ensuring a clean start next time.
    static unsigned long splash_start_time = 0;
    static bool sound_started = false;

    // Initialization step (runs only once)
    if (splash_start_time == 0) {
        splash_start_time = millis();
        sound_started = false;

        // Display splash screen
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(25, 25);
        display.println("Winamp");
        display.display();
    }

    // Check for BT disconnection
    if (!is_bt_connected) {
        Serial.println("BT disconnected during sample playback. Returning to discovery.");
        if (mp3File) mp3File.close();
        // Reset state for next time
        splash_start_time = 0;
        sound_started = false;
        initial_scan_complete = false; // Allow rescanning
        artists.clear(); // Clear artist data
        currentState = BT_DISCOVERY;
        return;
    }

    // After 6 seconds, start playing the sound (if not already started)
    if (millis() - splash_start_time >= 6000 && !sound_started) {
        play_file("/sample.mp3", true);
        sound_started = true;
    }

    // After 15 seconds, move to the next state
    if (millis() - splash_start_time >= 15000) {
        Serial.println("Splash screen finished.");

        // Stop sample playback if it's still going
        if (sound_started && mp3File) {
             a2dp.set_data_callback_in_frames(nullptr);
        }

        if (mp3File) {
            mp3File.close();
        }

        // Reset state for next time
        splash_start_time = 0;
        sound_started = false;

        playlist_scroll_offset = 0;
        ui_dirty = true;
        currentState = ARTIST_SELECTION;
    }
}

void scan_artists() {
    artists.clear();

    // Step 1: Always scan the SD card to get the current state
    std::vector<String> current_artists_on_sd;
    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open SD root");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        // Check if it's a potential artist directory
        if (file.isDirectory() && strcmp(file.name(), "data") != 0 && strcmp(file.name(), "System Volume Information") != 0 && file.name()[0] != '.') {
            String artist_path = "/" + String(file.name());
            File artist_dir = SD.open(artist_path);
            bool is_empty = true;
            if (artist_dir) {
                File album_file = artist_dir.openNextFile();
                while(album_file) {
                    // We're looking for at least one non-hidden subdirectory (album)
                    if (album_file.isDirectory() && album_file.name()[0] != '.') {
                        is_empty = false;
                        album_file.close(); // Found one, no need to check further
                        break;
                    }
                    album_file = artist_dir.openNextFile();
                }
                artist_dir.close();
            }

            if (!is_empty) {
                current_artists_on_sd.push_back(file.name());
            }
        }
        file = root.openNextFile();
    }
    root.close();

    // Step 2: Try to use the cache
    File cache_file = SD.open("/data/_artists.dat", FILE_READ);
    if (cache_file) {
        int cached_count = cache_file.readStringUntil('\n').toInt();

        // Step 3: Validate cache
        if (cached_count == current_artists_on_sd.size()) {
            Serial.println("Artist cache is valid. Loading from cache.");
            while (cache_file.available()) {
                String artist_name = cache_file.readStringUntil('\n');
                artist_name.trim(); // Remove potential whitespace/CRLF
                if (artist_name.length() > 0) {
                    artists.push_back(artist_name);
                }
            }
            cache_file.close();

            // Final check: if number of artists read from cache differs, something is wrong. Rescan.
            if (artists.size() == current_artists_on_sd.size()) {
                 return; // Cache loaded successfully
            }
            Serial.println("Cache file content mismatch. Re-scanning.");
            artists.clear(); // Clear potentially corrupt data
        }
        cache_file.close();
    }

    // Step 4: If cache is invalid/missing, use SD data and write new cache
    Serial.println("Artist cache invalid or missing. Using fresh scan data.");
    artists = current_artists_on_sd; // Use the data we scanned earlier

    SD.remove("/data/_artists.dat"); // Ensure clean slate
    cache_file = SD.open("/data/_artists.dat", FILE_WRITE);
    if (cache_file) {
        cache_file.println(artists.size());
        for (const auto& artist : artists) {
            cache_file.println(artist);
        }
        cache_file.close();
        Serial.println("New artist cache created.");
    } else {
        Serial.println("Failed to create artist cache file.");
    }
}

void draw_bt_discovery_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.print("Select BT Speaker:");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (bt_devices.empty()) {
        display.setCursor(0, 15 + 5);
        display.print("Scanning...");
    } else {
        display.setCursor(0, 15 + 5);
        for (int i = bt_discovery_scroll_offset; i < bt_devices.size() && i < bt_discovery_scroll_offset + 4; i++) {
            if (i == selected_bt_device) {
                display.print("> ");
            } else {
                display.print("  ");
            }
            display.print(bt_devices[i].name.c_str());
        }
    }
    display.display();
}

void scan_playlists(String artist_name) {
    playlists.clear();
    String artist_path = "/" + artist_name;

    // Step 1: Always scan the artist folder to get the current state
    std::vector<String> current_albums_on_sd;
    File artist_dir = SD.open(artist_path);
    if (!artist_dir) {
        Serial.printf("Failed to open artist directory: %s\n", artist_path.c_str());
        return;
    }

    File file = artist_dir.openNextFile();
    while(file) {
        // Find non-hidden directories (albums) that are not empty
        if (file.isDirectory() && file.name()[0] != '.') {
            String album_path = artist_path + "/" + file.name();
            File album_dir = SD.open(album_path);
            bool is_empty = true;
            if (album_dir) {
                File song_file = album_dir.openNextFile();
                while(song_file) {
                    if (!song_file.isDirectory() && String(song_file.name()).endsWith(".mp3")) {
                        is_empty = false;
                        song_file.close();
                        break;
                    }
                    song_file = album_dir.openNextFile();
                }
                album_dir.close();
            }
            if (!is_empty) {
                current_albums_on_sd.push_back(file.name());
            }
        }
        file = artist_dir.openNextFile();
    }
    artist_dir.close();

    // Step 2: Try to use the cache
    String cache_path = artist_path + "/_albums.dat";
    File cache_file = SD.open(cache_path, FILE_READ);
    if (cache_file) {
        int cached_count = cache_file.readStringUntil('\n').toInt();

        // Step 3: Validate cache
        if (cached_count == current_albums_on_sd.size()) {
            Serial.println("Album cache is valid. Loading from cache.");
            while (cache_file.available()) {
                String album_name = cache_file.readStringUntil('\n');
                album_name.trim();
                if (album_name.length() > 0) {
                    playlists.push_back(album_name);
                }
            }
            cache_file.close();
            if (playlists.size() == current_albums_on_sd.size()) {
                return; // Cache loaded successfully
            }
            Serial.println("Album cache file content mismatch. Re-scanning.");
            playlists.clear();
        }
        cache_file.close();
    }

    // Step 4: If cache is invalid/missing, use SD data and write new cache
    Serial.println("Album cache invalid or missing. Using fresh scan data.");
    playlists = current_albums_on_sd;

    SD.remove(cache_path);
    cache_file = SD.open(cache_path, FILE_WRITE);
    if (cache_file) {
        cache_file.println(playlists.size());
        for (const auto& playlist : playlists) {
            cache_file.println(playlist);
        }
        cache_file.close();
        Serial.println("New album cache created.");
    } else {
        Serial.println("Failed to create album cache file.");
    }
}

void draw_artist_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    draw_dynamic_text("Select Artist:", 0, 0, false, 0);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (artists.empty()) {
        display.setCursor(0, 26);
        display.print("No artists found!");
    } else {
        for (int i = artist_scroll_offset; i < artists.size() && i < artist_scroll_offset + 4; i++) {
            int y_pos = 26 + (i - artist_scroll_offset) * 10;
            String name = artists[i];
            int line_index = i - artist_scroll_offset + 1;
            if (i == selected_artist) {
                display.setCursor(0, y_pos);
                display.print("> ");
                draw_dynamic_text(name, y_pos, 12, true, line_index);
            } else {
                draw_dynamic_text(name, y_pos, 12, false, line_index);
            }
        }
    }
    display.display();
}

void handle_artist_selection() {
    if (artists.empty()) {
        scan_artists();
    }
    draw_artist_ui();
}


void draw_playlist_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    draw_dynamic_text("Select Playlist:", 0, 0, false, 0);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (playlists.empty()) {
        display.setCursor(0, 26);
        display.print("No playlists found!");
    } else {
        int list_size = playlists.size();
        for (int i = playlist_scroll_offset; i < list_size + 1 && i < playlist_scroll_offset + 4; i++) {
            int y_pos = 26 + (i - playlist_scroll_offset) * 10;
            int line_index = i - playlist_scroll_offset + 1;

            if (i == list_size) { // After the last playlist, show "back"
                if (i == selected_playlist) {
                    display.setCursor(0, y_pos);
                    display.print("> ");
                    draw_dynamic_text("<- back", y_pos, 12, true, line_index);
                } else {
                    draw_dynamic_text("<- back", y_pos, 12, false, line_index);
                }
            } else {
                String name = playlists[i];
                if (i == selected_playlist) {
                    display.setCursor(0, y_pos);
                    display.print("> ");
                    draw_dynamic_text(name, y_pos, 12, true, line_index);
                } else {
                    draw_dynamic_text(name, y_pos, 12, false, line_index);
                }
            }
        }
    }
    display.display();
}

void handle_playlist_selection() {
    if (playlists.empty()) {
        scan_playlists(artists[selected_artist]);
    }
    draw_playlist_ui();
}

void draw_player_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Header
    String artist_name = artists[selected_artist];
    draw_dynamic_text(artist_name, 0, 0, true, 0);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Currently Playing Song
    if (!current_playlist_files.empty()) {
        String playing_song = current_playlist_files[current_song_index];
        int last_slash = playing_song.lastIndexOf('/');
        if (last_slash != -1) {
            playing_song = playing_song.substring(last_slash + 1);
        }
        playing_song.replace(".mp3", "");
        draw_dynamic_text(">> " + playing_song, 12, 0, true, 1);
    }
    display.drawLine(0, 22, 127, 22, SSD1306_WHITE);

    // Playlist
    if (!current_playlist_files.empty()) {
        int list_size = current_playlist_files.size();
        for (int i = player_scroll_offset; i < list_size + 1 && i < player_scroll_offset + 4; i++) {
            int y_pos = 26 + (i - player_scroll_offset) * 10;
            int line_index = i - player_scroll_offset + 2;

            if (i == list_size) { // After the last song, show "back"
                if (i == selected_song_in_player) {
                    display.setCursor(0, y_pos);
                    display.print("> ");
                    draw_dynamic_text("<- back", y_pos, 12, true, line_index);
                } else {
                    draw_dynamic_text("<- back", y_pos, 12, false, line_index);
                }
            } else {
                String song_name = current_playlist_files[i];
                int last_slash = song_name.lastIndexOf('/');
                if (last_slash != -1) {
                    song_name = song_name.substring(last_slash + 1);
                }
                song_name.replace(".mp3", "");

                if (i == selected_song_in_player) {
                    display.setCursor(0, y_pos);
                    display.print("> ");
                    draw_dynamic_text(song_name, y_pos, 12, true, line_index);
                } else {
                    draw_dynamic_text(song_name, y_pos, 12, false, line_index);
                }
            }
        }
    }

    display.display();
}

void play_file(String filename, bool from_spiffs) {
    if (mp3File) {
        mp3File.close();
    }

    if (from_spiffs) {
        mp3File = SPIFFS.open(filename);
    } else {
        mp3File = SD.open(filename);
    }

    if (!mp3File) {
        Serial.printf("Failed to open file: %s\n", filename.c_str());
        return;
    }

    // Reset PCM buffer to prevent overflow from previous playback
    pcm_buffer_len = 0;
    decoder.setDataCallback(pcm_data_callback);
    a2dp.set_data_callback_in_frames(get_data_frames);
    Serial.printf("Playing %s from %s\n", filename.c_str(), from_spiffs ? "SPIFFS" : "SD");
}

void play_song(String filename) {
    play_file(filename, false);
    is_playing = true;
    song_started = true;
}

void handle_player() {
    if (!is_bt_connected) {
        Serial.println("BT disconnected during playback. Returning to discovery.");
        if (mp3File) mp3File.close();
        decoder.end();
        song_started = false;
        is_playing = false;
        initial_scan_complete = false; // Allow rescanning
        artists.clear(); // Clear artist data
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
        ui_dirty = true;
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