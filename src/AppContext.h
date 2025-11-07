#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <vector>
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <BluetoothA2DPSource.h>
#include <MP3DecoderHelix.h>
#include <SD.h>
#include "Button.h"

// Forward declaration
class StateManager;

enum FileType { MP3, WAV };

struct Song {
  String path;
  String artist;
  String album;
  FileType type;
};

class AppContext {
public:
    Adafruit_SSD1306& display;
    BluetoothA2DPSource& a2dp;
    Button& button;

    // --- Audio System ---
    libhelix::MP3DecoderHelix decoder;
    File audioFile;
    uint8_t read_buffer[1024];
    int16_t pcm_buffer[8192]; // Increased buffer size for stability
    volatile size_t pcm_buffer_head = 0;
    volatile size_t pcm_buffer_tail = 0;
    volatile size_t pcm_buffer_count = 0;
    volatile int diag_sample_rate = 0;
    volatile int diag_bits_per_sample = 0;
    volatile int diag_channels = 0;

    // --- Shared Data ---
    std::vector<String> artists;
    int selected_artist = 0;
    int artist_scroll_offset = 0;

    std::vector<String> playlists;
    int selected_playlist = 0;
    int playlist_scroll_offset = 0;

    std::vector<Song> current_playlist_files;
    int current_song_index = 0;
    int selected_song_in_player = 0;
    int player_scroll_offset = 0;
    unsigned long playback_position = 0;

    esp_bd_addr_t peer_address;
    bool ui_dirty = true;
    bool is_bt_connected = false;
    bool is_playing = false;
    volatile bool stop_requested = false; // Flag to safely stop audio task
    volatile char new_song_to_play[256] = ""; // Used to signal the audio task

    // --- Task Handles & Mutex for Core 1 ---
    TaskHandle_t audioTaskHandle = NULL;
    portMUX_TYPE pcm_buffer_mutex = portMUX_INITIALIZER_UNLOCKED;
    SemaphoreHandle_t audio_task_semaphore = NULL;
    TaskHandle_t wifiTaskHandle = NULL;
    volatile bool wifi_task_should_stop = false;
    volatile bool new_song_from_spiffs = false;
    bool sample_playback_is_active = false;

    // --- Marquee ---
    static const int MAX_MARQUEE_LINES = 6;
    bool is_marquee_active[MAX_MARQUEE_LINES] = {false};
    unsigned long marquee_start_time[MAX_MARQUEE_LINES] = {0};
    String marquee_text[MAX_MARQUEE_LINES];

    void* state_manager = nullptr; // Pointer to the StateManager

    AppContext(Adafruit_SSD1306& d, BluetoothA2DPSource& a, Button& b)
        : display(d), a2dp(a), button(b) {}
};

void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position = 0);
void stop_audio_playback(AppContext& context);
void draw_bitmap_from_spiffs(AppContext& context, const char *filename, int16_t x, int16_t y);

#endif // APP_CONTEXT_H
