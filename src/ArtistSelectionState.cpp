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

    return nullptr;
}

void ArtistSelectionState::exit(AppContext& context) {}

State* ArtistSelectionState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_artist++;
        if (context.selected_artist > context.artists.size()) {
            context.selected_artist = 0;
        }
        context.ui_dirty = true;
    } else if (is_scroll_button && !is_short_press) {
        if (context.selected_artist == context.artists.size()) {
            return new SettingsState();
        } else if (!context.artists.empty()) {
            context.playlists.clear();
            context.selected_playlist = 0;
            return new PlaylistSelectionState();
        }
    }
    return nullptr;
}

void ArtistSelectionState::scan_artists(AppContext& context) {
    File root = SD.open("/");
    if (!root) {
        Log::printf("Failed to open SD root\n");
        return;
    }

    while(true) {
        File file = root.openNextFile();
        if (!file) {
            break; // No more files
        }

        if (file.isDirectory() && strcmp(file.name(), "data") != 0 && strcmp(file.name(), "System Volume Information") != 0 && file.name()[0] != '.') {
            // Create a safe copy of the name before closing the file
            String artistName = String(file.name());
            context.artists.push_back(artistName);
        }

        file.close(); // Now it's safe to close
    }
    root.close();
}
