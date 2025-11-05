#include "ArtistSelectionState.h"
#include "AppContext.h"
#include "SettingsState.h"
#include "PlaylistSelectionState.h"
#include <SD.h>
#include "pins.h"

void ArtistSelectionState::enter(AppContext& context) {
    if (context.artists.empty()) {
        scan_artists(context);
    }
}

State* ArtistSelectionState::loop(AppContext& context) {
    // Button handling
    bool current_scroll = !digitalRead(BTN_SCROLL);
    static bool scroll_pressed = false;
    static unsigned long scroll_press_time = 0;
    static bool scroll_long_press_triggered = false;

    if (current_scroll && !scroll_pressed) {
        scroll_pressed = true;
        scroll_press_time = millis();
        scroll_long_press_triggered = false;
    } else if (!current_scroll && scroll_pressed) {
        scroll_pressed = false;
        if (!scroll_long_press_triggered) {
            return handle_button_press(context, true, true);
        }
    }
    if (scroll_pressed && !scroll_long_press_triggered && (millis() - scroll_press_time >= 1000)) {
        scroll_long_press_triggered = true;
        return handle_button_press(context, false, true);
    }

    draw_artist_ui(context);
    return nullptr;
}

void ArtistSelectionState::exit(AppContext& context) {}

State* ArtistSelectionState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_artist++;
        if (context.selected_artist >= context.artists.size() + 1) {
            context.selected_artist = 0;
        }
        // Simplified scrolling for now
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
