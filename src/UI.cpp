#include "UI.h"
#include "AppContext.h"
#include "icons.h"
#include <SPIFFS.h>
#include "StateManager.h"
#include "ArtistSelectionState.h"
#include "PlaylistSelectionState.h"
#include "PlayerState.h"

void draw_header(AppContext& context, const String& title) {
    context.display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    context.display.setTextSize(2);
    context.display.setTextColor(SSD1306_BLACK);
    int16_t x1, y1;
    uint16_t w, h;
    context.display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    context.display.setCursor((128 - w) / 2, 2);
    context.display.print(title);
    context.display.setTextColor(SSD1306_WHITE);
    context.display.setTextSize(1);
}

void draw_player_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();

    draw_header(context, "Now Playing");

    if (!context.current_playlist_files.empty()) {
        Song& current_song = context.current_playlist_files[context.current_song_index];
        String song_name = current_song.path;
        int last_slash = song_name.lastIndexOf('/');
        if (last_slash != -1) {
            song_name = song_name.substring(last_slash + 1);
        }
        song_name.replace(".mp3", "");
        song_name.replace(".wav", "");

        context.display.setTextSize(1);
        context.display.setCursor(0, 16);
        context.display.print(current_song.artist);
        context.display.setCursor(0, 26);
        context.display.print(current_song.album);
        context.display.setCursor(0, 36);
        context.display.print(song_name);

        // Progress bar
        int progress = 0;
        if (context.audioFile) {
            progress = (int)(((float)context.audioFile.position() / context.audioFile.size()) * 128);
        }
        context.display.drawRect(0, 48, 128, 5, SSD1306_WHITE);
        context.display.fillRect(0, 48, progress, 5, SSD1306_WHITE);

        // Volume and status icons
        context.display.drawBitmap(100, 16, volume_icon, 8, 6, SSD1306_WHITE);
        context.display.setCursor(110, 16);
        context.display.print(context.a2dp.get_volume());

        if (context.a2dp.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
            context.display.drawBitmap(100, 26, play_icon, 8, 6, SSD1306_WHITE);
        } else {
            context.display.drawBitmap(100, 26, pause_icon, 8, 6, SSD1306_WHITE);
        }

        // Playlist
        int list_size = context.current_playlist_files.size();
        int center_y = 60;
        int item_height = 10;
        for (int i = 0; i < list_size; i++) {
            int y_pos = center_y + (i - context.selected_song_in_player) * item_height;
            if (y_pos > 54 && y_pos < 128) {
                String name = context.current_playlist_files[i].path;
                last_slash = name.lastIndexOf('/');
                if (last_slash != -1) {
                    name = name.substring(last_slash + 1);
                }
                name.replace(".mp3", "");
                name.replace(".wav", "");

                if (i == context.selected_song_in_player) {
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

void draw_artist_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();

    draw_header(context, "Artists");

    if (context.artists.empty()) {
        context.display.setCursor(0, 26);
        context.display.print("No artists found!");
    } else {
        int list_size = context.artists.size();
        int center_y = 32;
        int item_height = 10;

        for (int i = 0; i < list_size + 1; i++) {
            int y_pos = center_y + (i - context.selected_artist) * item_height;

            if (y_pos > 12 && y_pos < 64) {
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
    }
    context.display.display();
}

void draw_playlist_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();

    draw_header(context, "Playlists");

    if (context.playlists.empty()) {
        context.display.setCursor(0, 26);
        context.display.print("No playlists found!");
    } else {
        int list_size = context.playlists.size();
        int center_y = 32;
        int item_height = 10;

        for (int i = 0; i < list_size + 1; i++) {
            int y_pos = center_y + (i - context.selected_playlist) * item_height;

            if (y_pos > 12 && y_pos < 64) {
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
    }
    context.display.display();
}

// Helper functions to read BMP metadata
static uint16_t read16(File &f) {
  uint16_t result;
  f.read((uint8_t *)&result, sizeof(result));
  return result;
}

static uint32_t read32(File &f) {
  uint32_t result;
  f.read((uint8_t *)&result, sizeof(result));
  return result;
}

void draw_bitmap_from_spiffs(AppContext& context, const char *filename, int16_t x, int16_t y) {
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.print(F("Failed to open "));
    Serial.println(filename);
    return;
  }

  // BMP header validation
  if (read16(file) != 0x4D42) { // "BM"
    Serial.println(F("Not a valid BMP file"));
    file.close();
    return;
  }

  // Read metadata
  file.seek(10);
  uint32_t dataOffset = read32(file);
  file.seek(18);
  int32_t width = read32(file);
  int32_t height = read32(file);
  file.seek(28);
  uint16_t bpp = read16(file);
  file.seek(34);
  uint32_t dataSize = read32(file);

  if (bpp != 24) {
    Serial.print(F("Unsupported BMP format: "));
    Serial.print(bpp);
    Serial.println(F(" bpp"));
    file.close();
    return;
  }

  // BMPs are stored bottom-up, and rows are padded to a 4-byte boundary.
  uint32_t rowSize = (width * 3 + 3) & ~3;
  uint8_t lineBuffer[rowSize];

  file.seek(dataOffset);

  for (int16_t j = 0; j < height; j++) {
    // The BMP is stored bottom-up, so we read from the end of the file and draw from the bottom up.
    int16_t dest_y = y + (height - 1 - j);
    file.seek(dataOffset + j * rowSize);
    file.read(lineBuffer, rowSize);

    for (int16_t i = 0; i < width; i++) {
      // Convert 24-bit color to monochrome
      // BMP format is BGR, not RGB.
      uint8_t b = lineBuffer[i * 3];
      uint8_t g = lineBuffer[i * 3 + 1];
      uint8_t r = lineBuffer[i * 3 + 2];

      // Use a simple luminance calculation
      if ((r * 0.299 + g * 0.587 + b * 0.114) > 128) {
        context.display.drawPixel(x + i, dest_y, SSD1306_WHITE);
      } else {
        context.display.drawPixel(x + i, dest_y, SSD1306_BLACK);
      }
    }
  }

  file.close();
}

void draw_current_state_ui(AppContext& context, StateManager* stateManager) {
    State* currentState = stateManager->getCurrentState();
    if (!currentState) return;

    switch (currentState->getType()) {
        case StateType::ARTIST_SELECTION:
            draw_artist_ui(context);
            break;
        case StateType::PLAYLIST_SELECTION:
            draw_playlist_ui(context);
            break;
        case StateType::PLAYER:
            draw_player_ui(context);
            break;
        default:
            // Other states draw their own UI
            break;
    }
}
