#include "PlayerState.h"
#include "AppContext.h"
#include "PlaylistSelectionState.h"
#include <SD.h>
#include "pins.h"
#include <MP3DecoderHelix.h>
#include "Log.h"

int32_t get_data_frames(Frame *frame, int32_t frame_count);
int32_t get_wav_data_frames(Frame *frame, int32_t frame_count);
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);

void PlayerState::enter(AppContext& context) {
    Log::printf("Entering Player State\n");
    context.decoder.begin();
    context.decoder.setDataCallback(pcm_data_callback);
    if (!context.current_playlist_files.empty()) {
        play_song(context, context.current_playlist_files[context.current_song_index]);
    }
}

State* PlayerState::loop(AppContext& context) {
    ButtonPress press = context.button.read();
    if (press == SHORT_PRESS) {
        return handle_button_press(context, true, true);
    } else if (press == LONG_PRESS) {
        return handle_button_press(context, false, true);
    }

    if (context.audioFile && !context.audioFile.available()) {
        context.current_song_index = (context.current_song_index + 1) % context.current_playlist_files.size();
        play_song(context, context.current_playlist_files[context.current_song_index]);
    }

    draw_player_ui(context);
    return nullptr;
}

void PlayerState::exit(AppContext& context) {
    context.decoder.end();
}

State* PlayerState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_song_in_player++;
        if (context.selected_song_in_player >= context.current_playlist_files.size() + 1) {
            context.selected_song_in_player = 0;
        }
        context.ui_dirty = true;
    } else if (is_scroll_button && !is_short_press) {
        if (context.selected_song_in_player == context.current_playlist_files.size()) {
            return new PlaylistSelectionState();
        } else {
            context.current_song_index = context.selected_song_in_player;
            play_song(context, context.current_playlist_files[context.current_song_index]);
        }
    }
    return nullptr;
}

void PlayerState::draw_player_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);

    context.display.setCursor(0, 0);
    context.display.print("Now Playing:");
    context.display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (!context.current_playlist_files.empty()) {
        String song_name = context.current_playlist_files[context.current_song_index].path;
        int last_slash = song_name.lastIndexOf('/');
        if (last_slash != -1) {
            song_name = song_name.substring(last_slash + 1);
        }
        song_name.replace(".mp3", "");
        song_name.replace(".wav", "");

        context.display.setCursor(0, 12);
        context.display.print(song_name);
    }

    context.display.display();
}

void PlayerState::play_song(AppContext& context, Song song, unsigned long seek_position) {
    if (song.type == MP3) {
        play_mp3(context, song.path, seek_position);
    } else if (song.type == WAV) {
        play_wav(context, song.path, seek_position);
    }
}

void PlayerState::play_mp3(AppContext& context, String filename, unsigned long seek_position) {
    if (context.audioFile) {
        context.audioFile.close();
    }
    context.audioFile = SD.open(filename);
    if (!context.audioFile) {
        Serial.printf("Failed to open file: %s\n", filename.c_str());
        return;
    }
    context.pcm_buffer_len = 0;
    context.decoder.begin();
    context.a2dp.set_data_callback_in_frames(get_data_frames);
}

void PlayerState::play_wav(AppContext& context, String filename, unsigned long seek_position) {
    // Simplified for now
}
