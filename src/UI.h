#ifndef UI_H
#define UI_H

#include "AppContext.h"

class StateManager; // Forward declaration

void draw_header(AppContext& context, const String& title);
void draw_player_ui(AppContext& context);
void draw_artist_ui(AppContext& context);
void draw_playlist_ui(AppContext& context);
void draw_current_state_ui(AppContext& context, StateManager* stateManager);

#endif // UI_H
