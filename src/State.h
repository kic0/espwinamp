#ifndef STATE_H
#define STATE_H

class AppContext; // Forward declaration

enum class StateType {
    ARTIST_SELECTION,
    PLAYLIST_SELECTION,
    PLAYER,
    BT_DISCOVERY,
    BT_CONNECTING,
    SAMPLE_PLAYBACK,
    SETTINGS
};

class State {
public:
    virtual ~State() {}
    virtual void enter(AppContext& context) = 0;
    virtual State* loop(AppContext& context) = 0; // Return new state or nullptr
    virtual void exit(AppContext& context) = 0;
    virtual StateType getType() const = 0;
};

#endif // STATE_H
