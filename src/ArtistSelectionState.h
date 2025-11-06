#ifndef ARTIST_SELECTION_STATE_H
#define ARTIST_SELECTION_STATE_H

#include "State.h"
#include <Arduino.h>

class ArtistSelectionState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;
    StateType getType() const override { return StateType::ARTIST_SELECTION; }

private:
    State* handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button);
    void scan_artists(AppContext& context);
};

#endif // ARTIST_SELECTION_STATE_H
