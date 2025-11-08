#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <BluetoothA2DPSource.h>
#include <MP3DecoderHelix.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "icons.h"
#include "esp_gap_bt_api.h"
#include <vector>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

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
volatile bool is_scanning = false;
volatile bool is_bt_connected = false;
volatile bool is_connecting = false;
unsigned long connection_start_time = 0;

// ---------- Artists ----------
std::vector<String> artists;
int selected_artist = 0;
int artist_scroll_offset = 0;

// ---------- Playlist ----------
enum FileType { MP3, WAV };

struct Song {
  String path;
  FileType type;
};

std::vector<String> playlists;
int selected_playlist = 0;
int playlist_scroll_offset = 0;
std::vector<Song> current_playlist_files;
int current_song_index = 0;
int selected_song_in_player = 0;
int player_scroll_offset = 0;
bool is_playing = false;
bool song_started = false;
bool sample_started = false;
bool ui_dirty = true;
int paused_song_index = -1;
unsigned long paused_song_position = 0;

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
File audioFile;
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
  BT_RECONNECTING,
  SAMPLE_PLAYBACK,
  ARTIST_SELECTION,
  PLAYLIST_SELECTION,
  PLAYER,
  SETTINGS
};
AppState currentState = STARTUP;
AppState previousState = STARTUP;


// ---------- Settings ----------
int selected_setting = 0;
bool wifi_ap_enabled = false;
AsyncWebServer server(80);
String wifi_ssid;
String wifi_password;


// forward declaration
int32_t get_data_frames(Frame *frame, int32_t frame_count);
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);
void bt_connection_state_cb(esp_a2d_connection_state_t state, void* ptr);

struct WavHeader {
    // RIFF Chunk
    char riff_header[4]; // "RIFF"
    uint32_t wav_size; // Size of the WAV file in bytes
    char wave_header[4]; // "WAVE"
    // Format Chunk
    char fmt_header[4]; // "fmt "
    uint32_t fmt_chunk_size; // Should be 16 for PCM
    uint16_t audio_format; // Should be 1 for PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate; // sample_rate * num_channels * bits_per_sample / 8
    uint16_t sample_alignment; // num_channels * bits_per_sample / 8
    uint16_t bit_depth;
    // Data Chunk
    char data_header[4]; // "data"
    uint32_t data_size; // Number of bytes in data.
};

bool parse_wav_header(String path, WavHeader &header) {
    File file = SD.open(path);
    if (!file) {
        Serial.printf("Failed to open WAV file: %s\n", path.c_str());
        return false;
    }

    if (file.readBytes((char*)&header, sizeof(WavHeader)) != sizeof(WavHeader)) {
        Serial.println("Failed to read WAV header");
        file.close();
        return false;
    }

    file.close();

    if (strncmp(header.riff_header, "RIFF", 4) != 0 || strncmp(header.wave_header, "WAVE", 4) != 0) {
        Serial.println("Invalid WAV file format");
        return false;
    }

    return true;
}


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
// WAV callback
int32_t get_wav_data_frames(Frame *frame, int32_t frame_count) {
    if (audioFile && audioFile.available()) {
        int bytes_to_read = frame_count * diag_channels * (diag_bits_per_sample / 8);
        int bytes_read = audioFile.read((uint8_t*)frame, bytes_to_read);
        return bytes_read / (diag_channels * (diag_bits_per_sample / 8));
    }
    return 0;
}

int32_t get_data_frames(Frame *frame, int32_t frame_count) {
    // If we don't have enough PCM data, read from file and decode
    if (pcm_buffer_len == 0) {
        if (audioFile && audioFile.available()) {
            int bytes_read = audioFile.read(read_buffer, sizeof(read_buffer));
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
void handle_bt_reconnecting();
void handle_sample_playback();
void handle_artist_selection();
void draw_artist_ui();
void handle_playlist_selection();
void draw_playlist_ui();
void handle_player();
void draw_player_ui();
void handle_settings();
void draw_settings_ui();
void draw_header(String title);
void play_file(String filename, bool from_spiffs, unsigned long seek_position = 0);
void play_wav(String filename, unsigned long seek_position = 0);
void play_mp3(String filename, unsigned long seek_position = 0);
void play_song(Song song, unsigned long seek_position = 0);
void draw_bitmap_from_spiffs(const char *filename, int16_t x, int16_t y);
void start_wifi_ap();
void stop_wifi_ap();

void calculate_scroll_offset(int &selected_item, int item_count, int &scroll_offset, int center_offset_ignored) {
    int display_lines = (currentState == PLAYER) ? 3 : 4;
    int center_offset = display_lines / 2;

    // Wrap selection
    if (selected_item >= item_count) {
        selected_item = 0;
    } else if (selected_item < 0) {
        selected_item = item_count - 1;
    }

    // Don't scroll if all items fit
    if (item_count <= display_lines) {
        scroll_offset = 0;
        return;
    }

    // Determine if we need to scroll
    if (selected_item < scroll_offset + center_offset) {
        scroll_offset = selected_item - center_offset;
    } else if (selected_item >= scroll_offset + display_lines - center_offset) {
        scroll_offset = selected_item - (display_lines - 1 - center_offset);
    }

    // Clamp scroll_offset to bounds
    if (scroll_offset < 0) {
        scroll_offset = 0;
    }
    if (scroll_offset > item_count - display_lines) {
        scroll_offset = item_count - display_lines;
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
        case BT_RECONNECTING:
            handle_bt_reconnecting();
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
        case SETTINGS:
            handle_settings();
            break;
    }
    delay(50);
}


void handle_button_press(bool is_short_press, bool is_scroll_button) {
    Serial.printf("Button press: short=%d, scroll=%d, state=%d\n", is_short_press, is_scroll_button, currentState);

    if (currentState == BT_DISCOVERY) {
        if (is_scroll_button && is_short_press) { // Scroll with short press
            selected_bt_device++;
            calculate_scroll_offset(selected_bt_device, bt_devices.size() + 1, bt_discovery_scroll_offset, 2);
            ui_dirty = true;
        } else if (is_scroll_button && !is_short_press) { // Select with long press
            if (selected_bt_device == bt_devices.size()) {
                previousState = currentState;
                currentState = SETTINGS;
                selected_setting = 0; // Reset selection in the settings menu
                ui_dirty = true;
            } else if (!bt_devices.empty()) {
                DiscoveredBTDevice selected_device = bt_devices[selected_bt_device];
                Serial.printf("Selected device: %s\n", selected_device.name.c_str());

                // Allow a moment for any pending remote name requests to complete
                delay(1000);

                // Stop scanning
                esp_bt_gap_cancel_discovery();
                is_scanning = false;

                // Connect to the device
                is_connecting = true;
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
                    is_connecting = false;
                    // Go back to scanning
                    is_scanning = false;
                }
            }
        }
    } else if (currentState == ARTIST_SELECTION) {
        if (is_scroll_button && is_short_press) { // Scroll with short press
            selected_artist++;
            calculate_scroll_offset(selected_artist, artists.size() + 1, artist_scroll_offset, 2);
            for (int i=0; i<MAX_MARQUEE_LINES; ++i) is_marquee_active[i] = false;
            ui_dirty = true;
        } else if (is_scroll_button && !is_short_press) { // Select with long press
            if (selected_artist == artists.size()) {
                previousState = currentState;
                currentState = SETTINGS;
                selected_setting = 0; // Reset selection in the settings menu
                ui_dirty = true;
            } else if (!artists.empty()) {
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

                // Scan for mp3 and wav files in the selected playlist folder
                current_playlist_files.clear();
                File playlist_folder = SD.open(full_path);
                File file = playlist_folder.openNextFile();
                while(file) {
                    if (!file.isDirectory()) {
                        String fileName = String(file.name());
                        String lowerCaseFileName = fileName;
                        lowerCaseFileName.toLowerCase();
                        if (lowerCaseFileName.endsWith(".mp3")) {
                            current_playlist_files.push_back({full_path + "/" + fileName, MP3});
                        } else if (lowerCaseFileName.endsWith(".wav")) {
                            current_playlist_files.push_back({full_path + "/" + fileName, WAV});
                        }
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
                play_song(current_playlist_files[current_song_index], 0);
            }
        }
    } else if (currentState == SETTINGS) {
        if (is_scroll_button && is_short_press) {
            if (!wifi_ap_enabled) {
                selected_setting = (selected_setting + 1) % 2;
                ui_dirty = true;
            }
        } else if (is_scroll_button && !is_short_press) {
            if (wifi_ap_enabled) {
                // Long press while AP is active always means "back"
                stop_wifi_ap();
                currentState = previousState;
            } else {
                if (selected_setting == 0) { // "Enable WiFi AP" is selected
                    start_wifi_ap();
                } else { // "<- back" is selected
                    currentState = previousState;
                    ui_dirty = true;
                }
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
                is_scanning = false;
                Serial.println("BT discovery stopped.");
                if (currentState == BT_DISCOVERY) {
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
    if (is_connecting) {
        return; // Don't try to auto-connect if we're already connecting
    }
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
            esp_bt_gap_cancel_discovery();
            is_connecting = true;
            if (a2dp.connect_to(saved_addr)) {
                connection_start_time = millis();
                currentState = BT_CONNECTING;
                return;
            } else {
                Serial.println("Failed to connect to saved device.");
                is_connecting = false;
                is_scanning = false; // a new scan will start after the timeout
            }
        }
    }

    Serial.println("Saved device not found in scan results.");
}

void handle_bt_discovery() {
    if (is_connecting) {
        return; // Don't start a new scan if we're already trying to connect
    }
    // a scan is throttled to once every 12 seconds
    static unsigned long last_scan_time = -12000;

    if (!is_scanning && !is_connecting && millis() - last_scan_time > 12000) {
        Serial.println("Starting BT device discovery...");
        bt_devices.clear();
        selected_bt_device = 0; // a new scan is starting, lets reset the selection

        esp_bt_inq_mode_t mode = ESP_BT_INQ_MODE_GENERAL_INQUIRY;
        uint8_t inquiry_length = 10;
        uint8_t num_responses = 0;

        esp_bt_gap_start_discovery(mode, inquiry_length, num_responses);
        is_scanning = true;
        last_scan_time = millis();
    }
    draw_bt_discovery_ui();
}

void handle_bt_connecting() {
    display.clearDisplay();
    draw_header("Connecting...");
    display.display();

    if (is_bt_connected) {
        Serial.println("Connection established.");
        is_connecting = false;
        a2dp.set_volume(64); // Set volume to 50%
        if (paused_song_index != -1) {
            currentState = PLAYER;
        } else {
            currentState = SAMPLE_PLAYBACK;
        }
    } else if (millis() - connection_start_time > 15000) { // 15 second timeout
        Serial.println("Connection timeout. Returning to discovery.");
        is_bt_connected = false;
        is_connecting = false;
        currentState = BT_DISCOVERY;
    }
}

void handle_bt_reconnecting() {
    static unsigned long reconnect_start_time = 0;
    if (reconnect_start_time == 0) {
        reconnect_start_time = millis();
    }

    if (ui_dirty) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Reconnecting...");
        display.display();
        ui_dirty = false;
    }

    if (is_bt_connected) {
        Serial.println("Reconnection successful.");
        reconnect_start_time = 0; // Reset for next time
        currentState = PLAYER;
        ui_dirty = true;
    } else if (millis() - reconnect_start_time > 15000) { // 15 second timeout
        Serial.println("Reconnection timeout. Returning to discovery.");
        reconnect_start_time = 0; // Reset for next time
        is_connecting = false;
        a2dp.disconnect(); // try to kill any pending connection
        currentState = BT_DISCOVERY;
        ui_dirty = true;
    }
}


void handle_sample_playback() {
    static unsigned long splash_start_time = 0;
    static bool sound_started = false;

    // Initialization step
    if (splash_start_time == 0) {
        splash_start_time = millis();
        sound_started = false;
        ui_dirty = true; // Force a redraw on first entry
    }

    if (ui_dirty) {
        display.clearDisplay();
        draw_header("Winamp"); // Add a header for consistency
        draw_bitmap_from_spiffs("/splash.bmp", 10, 12); // Adjust y-pos for header
        display.display();
        ui_dirty = false;
    }

    // Check for BT disconnection
    if (!is_bt_connected) {
        Serial.println("BT disconnected during sample playback. Entering reconnecting state.");
        if (audioFile) audioFile.close();
        // Reset state for next time
        splash_start_time = 0;
        sound_started = false;
        currentState = BT_RECONNECTING;
        ui_dirty = true;
        return;
    }

    // After 5 seconds, start playing the sound (if not already started)
    if (millis() - splash_start_time >= 5000 && !sound_started) {
        play_file("/sample.mp3", true);
        sound_started = true;
        is_playing = true;
        song_started = true; // Use song_started to be consistent with main player
    }

    bool song_finished = sound_started && audioFile && !audioFile.available();
    bool timeout_reached = millis() - splash_start_time >= 20000;

    // Transition when song finishes or timeout is reached
    if (song_finished || timeout_reached) {
        if(song_finished) Serial.println("Sample playback finished.");
        if(timeout_reached) Serial.println("Sample playback timed out.");

        // Stop sample playback if it's still going
        if (sound_started && audioFile) {
             a2dp.set_data_callback_in_frames(nullptr);
        }

        if (audioFile) {
            audioFile.close();
        }

        // Reset state for next time
        splash_start_time = 0;
        sound_started = false;
        is_playing = false;
        song_started = false;

        // Transition to the next state
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

void draw_header(String title) {
    display.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(2, 2);
    display.print(title);

    if (is_bt_connected) {
        display.drawBitmap(SCREEN_WIDTH - 20, 1, bt_icon, 8, 8, SSD1306_BLACK);
    }
    if (is_playing) {
        display.drawBitmap(SCREEN_WIDTH - 10, 1, play_icon, 8, 8, SSD1306_BLACK);
    }

    display.setTextColor(SSD1306_WHITE); // Reset text color for the rest of the UI
}

void draw_bt_discovery_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;
    display.clearDisplay();
    draw_header("Select BT Speaker");

    if (bt_devices.empty()) {
        display.setCursor(0, 26);
        display.print("Scanning...");
    } else {
        int list_size = bt_devices.size();
        for (int i = bt_discovery_scroll_offset; i < list_size + 1 && i < bt_discovery_scroll_offset + 4; i++) {
            int y_pos = 12 + (i - bt_discovery_scroll_offset) * 10;
            if (i == list_size) { // "Settings" option
                if (i == selected_bt_device) {
                    display.setCursor(0, y_pos);
                    display.print("> ");
                }
                draw_dynamic_text("-> Settings", y_pos, 12, false, i - bt_discovery_scroll_offset + 1);
            } else {
                display.setCursor(0, y_pos);
                if (i == selected_bt_device) {
                    display.print("> ");
                } else {
                    display.print("  ");
                }
                display.print(bt_devices[i].name.c_str());
            }
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
                    if (!song_file.isDirectory()) {
                        String fileName = String(song_file.name());
                        String lowerCaseFileName = fileName;
                        lowerCaseFileName.toLowerCase();
                        if (lowerCaseFileName.endsWith(".mp3") || lowerCaseFileName.endsWith(".wav")) {
                            is_empty = false;
                            song_file.close();
                            break;
                        }
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
    draw_header("Select Artist");

    if (artists.empty()) {
        display.setCursor(0, 26);
        display.print("No artists found!");
    } else {
        int list_size = artists.size();
        for (int i = artist_scroll_offset; i < list_size + 1 && i < artist_scroll_offset + 4; i++) {
            int y_pos = 12 + (i - artist_scroll_offset) * 10;
            String name;
            if (i == list_size) {
                name = "-> Settings";
            } else {
                name = artists[i];
            }
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
    if (!is_bt_connected) {
        Serial.println("BT disconnected during artist selection. Entering reconnecting state.");
        currentState = BT_RECONNECTING;
        ui_dirty = true;
        return;
    }
    if (artists.empty()) {
        scan_artists();
    }
    draw_artist_ui();
}


void draw_playlist_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;
    display.clearDisplay();
    draw_header("Select Playlist");

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
    if (!is_bt_connected) {
        Serial.println("BT disconnected during playlist selection. Entering reconnecting state.");
        currentState = BT_RECONNECTING;
        ui_dirty = true;
        return;
    }
    if (playlists.empty()) {
        scan_playlists(artists[selected_artist]);
    }
    draw_playlist_ui();
}

void draw_player_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;

    display.clearDisplay();
    draw_header("Now Playing");

    // Header
    String artist_name = artists[selected_artist];
    String album_name = playlists[selected_playlist];
    String header_text = artist_name + " - " + album_name;
    draw_dynamic_text(header_text, 12, 0, true, 0);
    display.drawLine(0, 22, 127, 22, SSD1306_WHITE);

    // Currently Playing Song
    if (!current_playlist_files.empty()) {
        String playing_song = current_playlist_files[current_song_index].path;
        int last_slash = playing_song.lastIndexOf('/');
        if (last_slash != -1) {
            playing_song = playing_song.substring(last_slash + 1);
        }
        playing_song.replace(".mp3", "");
        playing_song.replace(".wav", "");
        draw_dynamic_text(">> " + playing_song, 24, 0, true, 1);
    }
    display.drawLine(0, 34, 127, 34, SSD1306_WHITE);

    // Playlist
    if (!current_playlist_files.empty()) {
        int list_size = current_playlist_files.size();
        for (int i = player_scroll_offset; i < list_size + 1 && i < player_scroll_offset + 3; i++) {
            int y_pos = 38 + (i - player_scroll_offset) * 10;
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
                String song_name = current_playlist_files[i].path;
                int last_slash = song_name.lastIndexOf('/');
                if (last_slash != -1) {
                    song_name = song_name.substring(last_slash + 1);
                }
                song_name.replace(".mp3", "");
                song_name.replace(".wav", "");

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

void play_file(String filename, bool from_spiffs, unsigned long seek_position) {
    if (audioFile) {
        audioFile.close();
    }

    if (from_spiffs) {
        audioFile = SPIFFS.open(filename);
    } else {
        audioFile = SD.open(filename);
    }

    if (!audioFile) {
        Serial.printf("Failed to open file: %s\n", filename.c_str());
        return;
    }

    if (seek_position > 0) {
        if (audioFile.seek(seek_position)) {
            Serial.printf("Resuming from position %lu\n", seek_position);
            // a small buffer to scan for the sync word
            char sync_buf[3];
            // the MP3 sync word is 0xFFF*, so we check for the first 12 bits
            while (audioFile.available()) {
                int byte1 = audioFile.read();
                if (byte1 == 0xFF) {
                    int byte2 = audioFile.read();
                    if ((byte2 & 0xE0) == 0xE0) {
                        // MP3 sync word found, seek back two bytes and start playing
                        audioFile.seek(audioFile.position() - 2);
                        break;
                    }
                }
            }
        } else {
            Serial.printf("Failed to seek to position %lu\n", seek_position);
        }
    }

    // Reset PCM buffer to prevent overflow from previous playback
    pcm_buffer_len = 0;
    decoder.begin();
    decoder.setDataCallback(pcm_data_callback);
    a2dp.set_data_callback_in_frames(get_data_frames);
    Serial.printf("Playing %s from %s\n", filename.c_str(), from_spiffs ? "SPIFFS" : "SD");
}

void play_wav(String filename, unsigned long seek_position) {
    if (audioFile) {
        audioFile.close();
    }

    WavHeader header;
    if (!parse_wav_header(filename, header)) {
        return;
    }

    audioFile = SD.open(filename);
    if (!audioFile) {
        Serial.printf("Failed to open file: %s\n", filename.c_str());
        return;
    }

    // Seek past the header
    audioFile.seek(sizeof(WavHeader));
    if (seek_position > 0) {
        audioFile.seek(seek_position + sizeof(WavHeader));
    }

    diag_sample_rate = header.sample_rate;
    diag_bits_per_sample = header.bit_depth;
    diag_channels = header.num_channels;

    a2dp.set_data_callback_in_frames(get_wav_data_frames);
    Serial.printf("Playing WAV file: %s\n", filename.c_str());
    is_playing = true;
    song_started = true;
}

void play_mp3(String filename, unsigned long seek_position) {
    play_file(filename, false, seek_position);
    is_playing = true;
    song_started = true;
}

void play_song(Song song, unsigned long seek_position) {
    if (song.type == MP3) {
        play_mp3(song.path, seek_position);
    } else if (song.type == WAV) {
        play_wav(song.path, seek_position);
    }
}

void handle_player() {
    if (!is_bt_connected) {
        Serial.println("BT disconnected during playback. Entering reconnecting state.");
        if (audioFile) {
            paused_song_index = current_song_index;
            paused_song_position = audioFile.position();
            audioFile.close();
            Serial.printf("Pausing song %d at position %lu\n", paused_song_index, paused_song_position);
        }
        decoder.end();
        song_started = false;
        is_playing = false;
        currentState = BT_RECONNECTING;
        ui_dirty = true;
        return;
    }

    if (!song_started) {
        if (paused_song_index != -1) {
            current_song_index = paused_song_index;
            play_song(current_playlist_files[current_song_index], paused_song_position);
            paused_song_index = -1;
            paused_song_position = 0;
        } else {
            play_song(current_playlist_files[current_song_index], 0);
        }
    }

    // The audio data is now handled by the a2dp_data_callback.
    // We just need to check if the file has finished and play the next one.
    if (is_playing && audioFile && !audioFile.available()) {
        Serial.println("Song finished, playing next.");
        current_song_index++;
        if (current_song_index >= current_playlist_files.size()) {
            current_song_index = 0;
        }
        play_song(current_playlist_files[current_song_index], 0);
        ui_dirty = true;
    }

    draw_player_ui();
}

void draw_settings_ui() {
    if (!ui_dirty) return;
    ui_dirty = false;

    display.clearDisplay();
    draw_header("Settings");

    if (wifi_ap_enabled) {
        display.drawBitmap(SCREEN_WIDTH - 30, 1, wifi_icon, 8, 8, SSD1306_BLACK);
        display.setCursor(0, 12);
        display.println("WiFi AP Enabled");
        display.setCursor(0, 22);
        display.print("SSID: ");
        display.println(wifi_ssid);
        display.setCursor(0, 32);
        display.print("Pass: ");
        display.println(wifi_password);
        display.setCursor(0, 42);
        display.print("IP: ");
        display.println(WiFi.softAPIP().toString());
        display.setCursor(0, 54);
        display.print("> <- back");
    } else {
        display.setCursor(0, 12);
        if (selected_setting == 0) display.print("> ");
        display.print("Enable WiFi AP");

        display.setCursor(0, 22);
        if (selected_setting == 1) display.print("> ");
        display.print("<- back");
    }
    display.display();
}


void handle_settings() {
    // Web server is managed in the state transition.
    // We just need to draw the UI.
    draw_settings_ui();
}


// Helper function to read a 16-bit value from a file
uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

// Helper function to read a 32-bit value from a file
uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void draw_bitmap_from_spiffs(const char *filename, int16_t x, int16_t y) {
  File bmpFile;
  int bmpWidth, bmpHeight;
  uint8_t bmpDepth;
  uint32_t bmpImageoffset;
  uint8_t sdbuffer[3 * SCREEN_WIDTH]; // 3 * 128 = 384

  if ((x >= display.width()) || (y >= display.height()))
    return;

  bmpFile = SPIFFS.open(filename, "r");
  if (!bmpFile) {
    Serial.print("File not found: ");
    Serial.println(filename);
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) != 0x4D42) {
    Serial.println("Invalid BMP signature");
    return;
  }

  read32(bmpFile); // File size
  read32(bmpFile); // Creator
  bmpImageoffset = read32(bmpFile);
  read32(bmpFile); // Header size
  bmpWidth = read32(bmpFile);
  bmpHeight = read32(bmpFile);

  if (read16(bmpFile) != 1) {
    Serial.println("Unsupported BMP format (planes)");
    return;
  }

  bmpDepth = read16(bmpFile);
  if ((bmpDepth != 24) || (read32(bmpFile) != 0)) { // 24-bit, no compression
    Serial.println("Unsupported BMP format (depth or compression)");
    return;
  }

  // BMP rows are padded (if needed) to 4-byte boundary
  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3;

  // If bmpHeight is negative, image is in top-down order.
  // This is not common but has been observed in the wild.
  bool flip = true;
  if (bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip = false;
  }

  // Crop area to be loaded
  int w = bmpWidth;
  int h = bmpHeight;
  if ((x + w - 1) >= display.width())
    w = display.width() - x;
  if ((y + h - 1) >= display.height())
    h = display.height() - y;

  for (int j = 0; j < h; j++) {
    int row = flip ? bmpHeight - 1 - j : j;
    bmpFile.seek(bmpImageoffset + row * rowSize);
    if (bmpFile.read(sdbuffer, sizeof(sdbuffer)) != sizeof(sdbuffer)) {
      // Serial.println("file.read failed");
    }
    for (int i = 0; i < w; i++) {
      // Convert 24-bit color to 1-bit for OLED
      uint8_t r = sdbuffer[i * 3 + 2];
      uint8_t g = sdbuffer[i * 3 + 1];
      uint8_t b = sdbuffer[i * 3];
      if ((r + g + b) > 128 * 3) {
        display.drawPixel(x + i, y + j, SSD1306_WHITE);
      }
    }
  }
  bmpFile.close();
}

void start_wifi_ap() {
    // Stop Bluetooth
    a2dp.end(true);
    delay(1000);
    esp_bt_controller_deinit();

    // Start WiFi
    File file = SPIFFS.open("/wifi_credentials.txt", "r");
    if (file) {
        String ssid_line = file.readStringUntil('\n');
        String pass_line = file.readStringUntil('\n');
        file.close();

        wifi_ssid = ssid_line.substring(ssid_line.indexOf('=') + 1);
        wifi_password = pass_line.substring(pass_line.indexOf('=') + 1);
        wifi_ssid.trim();
        wifi_password.trim();

        WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str());

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            String html = "<html><body>";
            html += "<h1>Winamp ESP32 File Manager</h1>";
            html += "<form action='/upload' method='post' enctype='multipart/form-data'><input type='file' name='upload'><input type='submit' value='Upload'></form>";
            html += "<h2>Files on SD Card:</h2>";
            File root = SD.open("/");
            File file = root.openNextFile();
            while(file){
                html += "<p>";
                html += file.name();
                html += " <a href='/delete?file=";
                html += file.name();
                html += "'>[Delete]</a></p>";
                file = root.openNextFile();
            }
            root.close();
            html += "</body></html>";
            request->send(200, "text/html", html);
        });

        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
            request->send(200);
        }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            if(!index){
                String file_path = "/" + filename;
                File file = SD.open(file_path, FILE_WRITE);
                if(file){
                    request->_tempFile = file;
                }
            }
            if(len){
                request->_tempFile.write(data, len);
            }
            if(final){
                request->_tempFile.close();
            }
        });

        server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
            if(request->hasParam("file")){
                String file_path = "/" + request->getParam("file")->value();
                SD.remove(file_path);
            }
            request->redirect("/");
        });

        server.begin();
    }
    wifi_ap_enabled = true;
    ui_dirty = true;
}

void stop_wifi_ap() {
    server.end();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifi_ap_enabled = false;

    // Re-initialize Bluetooth
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    a2dp.start("winamp");

    ui_dirty = true;
}


void bt_connection_state_cb(esp_a2d_connection_state_t state, void* ptr){
    Serial.printf("A2DP connection state changed: %d\n", state);
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        is_bt_connected = true;
    } else {
        is_bt_connected = false;
        is_connecting = false; // Ensure we can scan again if disconnected
    }
}
