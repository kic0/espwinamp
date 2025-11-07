#include "PlayerState.h"
#include "AppContext.h"
#include "PlaylistSelectionState.h"
#include <SD.h>
#include "pins.h"
#include <MP3DecoderHelix.h>
#include "Log.h"
#include "UI.h"

int32_t get_data_frames(Frame *frame, int32_t frame_count);
int32_t get_wav_data_frames(Frame *frame, int32_t frame_count);
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);

void PlayerState::enter(AppContext& context) {
    Log::printf("Entering Player State\n");
    if (context.current_playlist_files.empty()) {
        scan_playlist_files(context);
    }
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

    for (int i = 0; i < AppContext::MAX_MARQUEE_LINES; i++) {
        if (context.is_marquee_active[i]) {
            context.ui_dirty = true;
            break;
        }
    }

    return nullptr;
}

void PlayerState::exit(AppContext& context) {
    context.decoder.end();
}

State* PlayerState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_song_in_player++;
        if (context.selected_song_in_player > context.current_playlist_files.size()) {
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

void PlayerState::play_song(AppContext& context, Song song, unsigned long seek_position) {
    if (song.type == MP3) {
        play_mp3(context, song.path, seek_position);
    } else if (song.type == WAV) {
        play_wav(context, song.path, seek_position);
    }
}

void PlayerState::play_mp3(AppContext& context, String filename, unsigned long seek_position) {
    play_file(context, filename, false, seek_position);
}

void PlayerState::play_wav(AppContext& context, String filename, unsigned long seek_position) {
    // Simplified for now
}

void PlayerState::scan_playlist_files(AppContext& context) {
    String artist = context.artists[context.selected_artist];
    String playlist = context.playlists[context.selected_playlist];
    String path = "/" + artist + "/" + playlist;

    File dir = SD.open(path);
    if (!dir) {
        Log::printf("Failed to open playlist directory: %s\n", path.c_str());
        return;
    }

    while (true) {
        File entry =  dir.openNextFile();
        if (! entry) {
            break;
        }
        String fileName = entry.name();
        fileName.toLowerCase();
        if (fileName.endsWith(".mp3") || fileName.endsWith(".wav")) {
            Song song;
            song.path = path + "/" + String(entry.name()); // Construct the full path
            song.artist = artist;
            song.album = playlist;
            song.type = fileName.endsWith(".mp3") ? MP3 : WAV;
            context.current_playlist_files.push_back(song);
        }
        entry.close();
    }
    dir.close();
}
