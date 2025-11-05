#include "PlaylistSelectionState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include "PlayerState.h"
#include <SD.h>
#include "pins.h"

void PlaylistSelectionState::enter(AppContext& context) {
    if (context.playlists.empty()) {
        scan_playlists(context);
    }
}

State* PlaylistSelectionState::loop(AppContext& context) {
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

    draw_playlist_ui(context);
    return nullptr;
}

void PlaylistSelectionState::exit(AppContext& context) {}

State* PlaylistSelectionState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        context.selected_playlist++;
        if (context.selected_playlist >= context.playlists.size() + 1) {
            context.selected_playlist = 0;
        }
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
                        context.current_playlist_files.push_back({full_path + "/" + fileName, MP3});
                    } else if (lowerCaseFileName.endsWith(".wav")) {
                        context.current_playlist_files.push_back({full_path + "/" + fileName, WAV});
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

void PlaylistSelectionState::draw_playlist_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);

    context.display.setCursor(0, 0);
    context.display.print("Select Playlist:");
    context.display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (context.playlists.empty()) {
        context.display.setCursor(0, 26);
        context.display.print("No playlists found!");
    } else {
        int list_size = context.playlists.size();
        for (int i = context.playlist_scroll_offset; i < list_size + 1 && i < context.playlist_scroll_offset + 4; i++) {
            int y_pos = 26 + (i - context.playlist_scroll_offset) * 10;

            if (i == list_size) {
                if (i == context.selected_playlist) {
                    context.display.setCursor(0, y_pos);
                    context.display.print("> ");
                    context.display.print("<- back");
                } else {
                    context.display.setCursor(12, y_pos);
                    context.display.print("<- back");
                }
            } else {
                String name = context.playlists[i];
                if (i == context.selected_playlist) {
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
