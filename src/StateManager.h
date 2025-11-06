#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "State.h"
#include "AppContext.h"

class StateManager {
public:
    StateManager(AppContext& ctx);
    void setState(State* newState);
    void loop();
    State* getCurrentState() { return currentState; }

private:
    State* currentState;
    AppContext& context;
};

#endif // STATE_MANAGER_H
