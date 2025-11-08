#ifndef SAMPLE_PLAYBACK_STATE_H
#define SAMPLE_PLAYBACK_STATE_H

#include "State.h"

class SamplePlaybackState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;
    StateType getType() const override { return StateType::SAMPLE_PLAYBACK; }

private:
    unsigned long start_time;
    bool playback_attempted = false;
    bool playback_confirmed = false;
};

#endif // SAMPLE_PLAYBACK_STATE_H
