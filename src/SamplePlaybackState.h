#ifndef SAMPLE_PLAYBACK_STATE_H
#define SAMPLE_PLAYBACK_STATE_H

#include "State.h"

class SamplePlaybackState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;

private:
    unsigned long start_time;
};

#endif // SAMPLE_PLAYBACK_STATE_H
