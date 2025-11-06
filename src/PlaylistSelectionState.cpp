#include "PlaylistSelectionState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include "PlayerState.h"
#include <SD.h>
#include "pins.h"
#include "Log.h"
#include "UI.h"

void PlaylistSelectionState::enter(AppContext& context) {
    Log::printf("Entering Playlist Selection State\n");
    if (context.playlists.empty()) {
        scan_playlists(context);
    }
}

State* PlaylistSelectionState::loop(AppContext& context) {
    ButtonPress press = context.button.read();
    if (press == SHORT_PRESS) {
        return handle_button_press(context, true, true);
    } else if (press == LONG_PRESS) {
        return handle_button_press(context, false, true);
    }

    for (int i = 0; i < AppContext::MAX_MARQUEE_LINES; i++) {
        if (context.is_marquee_active[i]) {
            context.ui_dirty = true;
            break;
        }
    }
    draw_playlist_ui(context);
    return nullptr;
}

void PlaylistSelectionState::exit(AppContext& context) {}

State* PlaylistSelectionState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_playlist = (context.selected_playlist + 1) % (context.playlists.size() + 1);
        context.ui_dirty = true;
    } else if (is_scroll_button && !is_short_press) {
        if (context.selected_playlist == context.playlists.size()) {
            return new ArtistSelectionState();
        } else if (!context.playlists.empty()) {
            context.current_playlist_files.clear();
            String artist_name = context.artists[context.selected_artist];
            String playlist_name = context.playlists[context.selected_playlist];
            String full_path = "/" + artist_name + "/" + playlist_name;

            File playlist_folder = SD.open(full_path);
            File file = playlist_folder.openNextFile();
            while(file) {
                if (!file.isDirectory()) {
                    String fileName = String(file.name());
                    String lowerCaseFileName = fileName;
                    lowerCaseFileName.toLowerCase();
                    if (lowerCaseFileName.endsWith(".mp3")) {
                        context.current_playlist_files.push_back({full_path + "/" + fileName, artist_name, playlist_name, MP3});
                    } else if (lowerCaseFileName.endsWith(".wav")) {
                        context.current_playlist_files.push_back({full_path + "/" + fileName, artist_name, playlist_name, WAV});
                    }
                }
                file.close();
                file = playlist_folder.openNextFile();
            }
            playlist_folder.close();

            if (!context.current_playlist_files.empty()) {
                context.current_song_index = 0;
                context.selected_song_in_player = 0;
                context.player_scroll_offset = 0;
                return new PlayerState();
            }
        }
    }
    return nullptr;
}

void PlaylistSelectionState::scan_playlists(AppContext& context) {
    String artist_name = context.artists[context.selected_artist];
    String artist_path = "/" + artist_name;
    File artist_dir = SD.open(artist_path);
    if (!artist_dir) {
        Serial.printf("Failed to open artist directory: %s\n", artist_path.c_str());
        return;
    }

    File file = artist_dir.openNextFile();
    while(file) {
        if (file.isDirectory() && file.name()[0] != '.') {
            context.playlists.push_back(file.name());
        }
        file.close();
        file = artist_dir.openNextFile();
    }
    artist_dir.close();
}
