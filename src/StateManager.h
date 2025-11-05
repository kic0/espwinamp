#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "State.h"
#include "AppContext.h"

class StateManager {
private:
    State* currentState;
    AppContext& context;

public:
    StateManager(AppContext& ctx);
    void setState(State* newState);
    void loop();
};

#endif // STATE_MANAGER_H
