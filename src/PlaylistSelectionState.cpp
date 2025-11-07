#include "PlaylistSelectionState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include "PlayerState.h"
#include <SD.h>
#include "Log.h"
#include "UI.h"

void PlaylistSelectionState::enter(AppContext& context) {
    Log::printf("Entering Playlist Selection State\n");
    if (context.playlists.empty()) {
        scan_playlists(context, context.artists[context.selected_artist]);
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

    return nullptr;
}

void PlaylistSelectionState::exit(AppContext& context) {}

State* PlaylistSelectionState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_playlist++;
        if (context.selected_playlist > context.playlists.size()) {
            context.selected_playlist = 0;
        }
        context.ui_dirty = true;
    } else if (is_scroll_button && !is_short_press) {
        if (context.selected_playlist == context.playlists.size()) {
            context.artists.clear(); // Clear artists before going back
            context.selected_artist = 0;
            return new ArtistSelectionState();
        } else if (!context.playlists.empty()) {
            context.current_playlist_files.clear();
            context.current_song_index = 0;
            context.selected_song_in_player = 0;
            return new PlayerState();
        }
    }
    return nullptr;
}

void PlaylistSelectionState::scan_playlists(AppContext& context, String artist) {
    String path = "/" + artist;
    File root = SD.open(path.c_str());
    if (!root) {
        Log::printf("Failed to open artist folder: %s\n", path.c_str());
        return;
    }

    while (true) {
        File file = root.openNextFile();
        if (!file) {
            break; // No more files
        }

        if (file.isDirectory() && file.name()[0] != '.') {
            // Create a safe copy of the name before closing the file
            String playlistName = String(file.name());
            context.playlists.push_back(playlistName);
        }

        file.close(); // Now it's safe to close
    }
    root.close();
}
