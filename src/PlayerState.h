#ifndef PLAYER_STATE_H
#define PLAYER_STATE_H

#include "State.h"
#include "AppContext.h" // For Song struct and AppContext
#include <Arduino.h>

class PlayerState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;

private:
    State* handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button);
    void play_song(AppContext& context, Song song, unsigned long seek_position = 0);
    void play_mp3(AppContext& context, String filename, unsigned long seek_position = 0);
    void play_wav(AppContext& context, String filename, unsigned long seek_position = 0);
};

#endif // PLAYER_STATE_H
