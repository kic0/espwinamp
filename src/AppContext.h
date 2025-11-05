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
    int16_t pcm_buffer[4096];
    int32_t pcm_buffer_len = 0;
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

    bool ui_dirty = true;

    AppContext(Adafruit_SSD1306& d, BluetoothA2DPSource& a, Button& b)
        : display(d), a2dp(a), button(b) {}
};

#endif // APP_CONTEXT_H
