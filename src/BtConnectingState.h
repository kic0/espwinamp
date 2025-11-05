#ifndef BT_CONNECTING_STATE_H
#define BT_CONNECTING_STATE_H

#include "State.h"

class BtConnectingState : public State {
public:
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;
};

#endif // BT_CONNECTING_STATE_H
