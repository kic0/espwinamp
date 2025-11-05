#include "ArtistSelectionState.h"
#include "AppContext.h"
#include "SettingsState.h"
#include "PlaylistSelectionState.h"
#include <SD.h>
#include "pins.h"
#include "Log.h"

void calculate_scroll_offset(int &selected_item, int item_count, int &scroll_offset, int center_offset);

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
        context.selected_artist++;
        calculate_scroll_offset(context.selected_artist, context.artists.size() + 1, context.artist_scroll_offset, 2);
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

void ArtistSelectionState::draw_artist_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);

    context.display.setCursor(0, 0);
    context.display.print("Select Artist:");
    context.display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (context.artists.empty()) {
        context.display.setCursor(0, 26);
        context.display.print("No artists found!");
    } else {
        int list_size = context.artists.size();
        for (int i = context.artist_scroll_offset; i < list_size + 1 && i < context.artist_scroll_offset + 4; i++) {
            int y_pos = 26 + (i - context.artist_scroll_offset) * 10;

            if (i == list_size) {
                if (i == context.selected_artist) {
                    context.display.setCursor(0, y_pos);
                    context.display.print("> ");
                    context.display.print("-> Settings");
                } else {
                    context.display.setCursor(12, y_pos);
                    context.display.print("-> Settings");
                }
            } else {
                String name = context.artists[i];
                if (i == context.selected_artist) {
                    context.display.setCursor(0, y_pos);
                    context.display.print("> ");
                    context.display.print(name);
                } else {
                    context.display.setCursor(12, y_pos);
                    context.display.print(name);
                }
            }
        }
    }
    context.display.display();
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
