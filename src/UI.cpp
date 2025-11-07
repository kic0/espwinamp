#include "UI.h"
#include "AppContext.h"
#include "icons.h"
#include <SPIFFS.h>
#include "StateManager.h"
#include "SettingsState.h"
#include "BtDiscoveryState.h"

// New, redesigned header function
void draw_header(AppContext& context, const String& title) {
    context.display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_BLACK);
    context.display.setCursor(2, 2);
    context.display.print(title);

    if (context.is_bt_connected) {
        context.display.setCursor(100, 2);
        context.display.print("BT");
    }
    if (context.a2dp.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
        context.display.setCursor(115, 2);
        context.display.print(">");
    }

    context.display.setTextColor(SSD1306_WHITE);
}

void draw_marquee_list_item(AppContext& context, int index, int x, int y, const String& text, bool selected) {
    int16_t x1, y1;
    uint16_t w, h;
    context.display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int marquee_x = x;
    if (w > 128 - x) {
        if (!context.is_marquee_active[index]) {
            context.is_marquee_active[index] = true;
            context.marquee_start_time[index] = millis();
            context.marquee_text[index] = text;
        }

        unsigned long elapsed = millis() - context.marquee_start_time[index];
        int scroll_pixels = w - (128 - x);
        int scroll_duration = scroll_pixels * 30;
        int total_duration = scroll_duration * 2 + 2000;

        int current_pos = elapsed % total_duration;

        if (current_pos < scroll_duration) {
            marquee_x = x - (current_pos / 30);
        } else if (current_pos < scroll_duration + 1000) {
            marquee_x = x - scroll_pixels;
        } else if (current_pos < scroll_duration * 2 + 1000) {
            marquee_x = x - scroll_pixels + ((current_pos - (scroll_duration + 1000)) / 30);
        } else {
            marquee_x = x;
        }
    } else {
        context.is_marquee_active[index] = false;
    }

    if (selected) {
        context.display.setCursor(0, y);
        context.display.print("> ");
    }

    context.display.setCursor(marquee_x, y);
    context.display.print(text);
}

void draw_list_ui(AppContext& context, const String& title, const std::vector<String>& items, int selected_item, const String& bottom_item_text) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();

    draw_header(context, title);

    if (items.empty() && bottom_item_text.isEmpty()) {
        context.display.setCursor(0, 26);
        context.display.print("No items found!");
    } else {
        int list_size = items.size();
        int center_y = 32;
        int item_height = 10;

        for(int i=0; i < AppContext::MAX_MARQUEE_LINES; i++) {
            context.is_marquee_active[i] = false;
        }

        for (int i = 0; i < list_size + (bottom_item_text.isEmpty() ? 0 : 1); i++) {
            int y_pos = center_y + (i - selected_item) * item_height;
            String item_text;
            bool is_bottom_item = (i == list_size);

            if (y_pos > 12 && y_pos < 64) {
                 if (is_bottom_item) {
                    item_text = bottom_item_text;
                } else {
                    item_text = items[i];
                }

                if (!item_text.isEmpty()) {
                   draw_marquee_list_item(context, i, 12, y_pos, item_text, i == selected_item);
                }
            }
        }
    }
    context.display.display();
}

void draw_player_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();

    String title = "Now Playing";
    if (!context.current_playlist_files.empty()) {
        Song& current_song = context.current_playlist_files[context.current_song_index];
        String song_name = current_song.path;
        int last_slash = song_name.lastIndexOf('/');
        if (last_slash != -1) {
            song_name = song_name.substring(last_slash + 1);
        }
        song_name.replace(".mp3", "");
        song_name.replace(".wav", "");
        title = song_name;
    }

    draw_header(context, title);

    if (!context.current_playlist_files.empty()) {
        Song& current_song = context.current_playlist_files[context.current_song_index];

        int progress = 0;
        if (context.audioFile) {
            progress = (int)(((float)context.audioFile.position() / context.audioFile.size()) * 128);
        }
        context.display.drawRect(0, 12, 128, 5, SSD1306_WHITE);
        context.display.fillRect(0, 12, progress, 5, SSD1306_WHITE);

        context.display.setCursor(2, 20);
        context.display.print(current_song.artist + " - " + current_song.album);

        int list_size = context.current_playlist_files.size();
        int center_y = 42;
        int item_height = 10;

        for(int i=0; i < AppContext::MAX_MARQUEE_LINES; i++) {
            context.is_marquee_active[i] = false;
        }

        for (int i = 0; i < list_size + 1; i++) {
            int y_pos = center_y + (i - context.selected_song_in_player) * item_height;
            String item_text;
            bool is_back_item = (i == list_size);

            if (y_pos > 30 && y_pos < 64) {
                if (is_back_item) {
                    item_text = "<- back";
                } else {
                    item_text = context.current_playlist_files[i].path;
                    int last_slash = item_text.lastIndexOf('/');
                    if (last_slash != -1) {
                        item_text = item_text.substring(last_slash + 1);
                    }
                    item_text.replace(".mp3", "");
                    item_text.replace(".wav", "");
                }
                draw_marquee_list_item(context, i, 12, y_pos, item_text, i == context.selected_song_in_player);
            }
        }
    }

    context.display.display();
}

void draw_settings_ui(AppContext& context) {
    StateManager* sm = (StateManager*)context.state_manager;
    SettingsState* state = (SettingsState*)sm->getCurrentState();

    std::vector<String> items;
    String bottom_item = "";

    if(state->is_ap_enabled()){
        items.push_back("Disable WiFi AP");
        items.push_back("SSID: " + state->get_ssid());
        items.push_back("Pass: " + state->get_password());
        items.push_back("IP: " + state->get_ip_address());
        bottom_item = "<- back";
    } else {
        items.push_back("Enable WiFi AP");
        bottom_item = "<- back";
    }

    draw_list_ui(context, "Settings", items, state->get_selected_item(), bottom_item);
}

void draw_bt_discovery_ui(AppContext& context) {
    StateManager* sm = (StateManager*)context.state_manager;
    BtDiscoveryState* state = (BtDiscoveryState*)sm->getCurrentState();

    std::vector<String> device_names;
    for (const auto& device : state->get_discovered_devices()) {
        device_names.push_back(device.name);
    }

    draw_list_ui(context, "BT Devices", device_names, state->get_selected_device(), "-> Settings");
}

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

  if (read16(file) != 0x4D42) {
    Serial.println(F("Not a valid BMP file"));
    file.close();
    return;
  }

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

  uint32_t rowSize = (width * 3 + 3) & ~3;
  uint8_t lineBuffer[rowSize];

  file.seek(dataOffset);

  for (int16_t j = 0; j < height; j++) {
    int16_t dest_y = y + (height - 1 - j);
    file.seek(dataOffset + j * rowSize);
    file.read(lineBuffer, rowSize);

    for (int16_t i = 0; i < width; i++) {
      uint8_t b = lineBuffer[i * 3];
      uint8_t g = lineBuffer[i * 3 + 1];
      uint8_t r = lineBuffer[i * 3 + 2];

      if ((r * 0.299 + g * 0.587 + b * 0.114) > 128) {
        context.display.drawPixel(x + i, dest_y, SSD1306_WHITE);
      } else {
        context.display.drawPixel(x + i, dest_y, SSD1306_BLACK);
      }
    }
  }

  file.close();
}

void draw_sample_playback_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();
    draw_bitmap_from_spiffs(context, "/splash.bmp", 10, 0);
    context.display.display();
}

void draw_connecting_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);
    context.display.setCursor(0, 0);
    context.display.println("Connecting...");
    context.display.display();
}
