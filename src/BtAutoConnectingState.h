#ifndef BT_AUTO_CONNECTING_STATE_H
#define BT_AUTO_CONNECTING_STATE_H

#include "State.h"

class BtAutoConnectingState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;
private:
    unsigned long start_time;
};

#endif // BT_AUTO_CONNECTING_STATE_H
