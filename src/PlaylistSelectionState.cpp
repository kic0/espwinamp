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

    // Drawing is now handled in the main loop
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
        Serial.println("Failed to open artist folder");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory() && file.name()[0] != '.') {
            context.playlists.push_back(file.name());
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}
