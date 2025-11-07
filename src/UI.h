#ifndef UI_H
#define UI_H

#include "AppContext.h"

void draw_header(AppContext& context, const String& title);
void draw_marquee_list_item(AppContext& context, int index, int x, int y, const String& text, bool selected);
void draw_player_ui(AppContext& context);
void draw_settings_ui(AppContext& context);
void draw_bt_discovery_ui(AppContext& context);
void draw_sample_playback_ui(AppContext& context);
void draw_connecting_ui(AppContext& context);
void draw_list_ui(AppContext& context, const String& title, const std::vector<String>& items, int selected_item, const String& bottom_item_text);

#endif // UI_H
