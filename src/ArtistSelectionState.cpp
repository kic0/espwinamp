#include "ArtistSelectionState.h"
#include "AppContext.h"
#include "SettingsState.h"
#include "PlaylistSelectionState.h"
#include <SD.h>
#include "pins.h"
#include "Log.h"
#include "UI.h"

void ArtistSelectionState::enter(AppContext& context) {
    Log::printf("Entering Artist Selection State\n");
    if (context.artists.empty()) {
        scan_artists(context);
    }
}

State* ArtistSelectionState::loop(AppContext& context) {
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
    draw_artist_ui(context);
    return nullptr;
}

void ArtistSelectionState::exit(AppContext& context) {}

State* ArtistSelectionState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_artist = (context.selected_artist + 1) % (context.artists.size() + 1);
        context.ui_dirty = true;
    } else if (is_scroll_button && !is_short_press) {
        if (context.selected_artist == context.artists.size()) {
            return new SettingsState();
        } else if (!context.artists.empty()) {
            context.playlists.clear();
            context.selected_playlist = 0;
            context.playlist_scroll_offset = 0;
            return new PlaylistSelectionState();
        }
    }
    return nullptr;
}

void ArtistSelectionState::scan_artists(AppContext& context) {
    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open SD root");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory() && strcmp(file.name(), "data") != 0 && strcmp(file.name(), "System Volume Information") != 0 && file.name()[0] != '.') {
            context.artists.push_back(file.name());
        }
        file.close(); // This was missing
        file = root.openNextFile();
    }
    root.close();
}
