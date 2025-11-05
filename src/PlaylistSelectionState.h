#ifndef PLAYLIST_SELECTION_STATE_H
#define PLAYLIST_SELECTION_STATE_H

#include "State.h"
#include <Arduino.h>

class PlaylistSelectionState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;

private:
    void draw_playlist_ui(AppContext& context);
    State* handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button);
    void scan_playlists(AppContext& context);
};

#endif // PLAYLIST_SELECTION_STATE_H
