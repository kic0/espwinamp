#ifndef STATE_H
#define STATE_H

#include "AppContext.h"

enum class StateType {
    NONE,
    SAMPLE_PLAYBACK,
    BT_DISCOVERY,
    BT_CONNECTING,
    ARTIST_SELECTION,
    PLAYLIST_SELECTION,
    PLAYER,
    SETTINGS
};

class State {
public:
    virtual ~State() {}
    virtual void enter(AppContext& context) = 0;
    virtual State* loop(AppContext& context) = 0;
    virtual void exit(AppContext& context) = 0;
    virtual StateType getType() const { return StateType::NONE; }
};

#endif // STATE_H
