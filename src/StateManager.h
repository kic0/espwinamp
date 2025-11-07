#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "State.h"
#include "AppContext.h"

class StateManager {
public:
    StateManager(AppContext& ctx);
    void setState(State* newState);
    void requestStateChange(State* newState); // New method for callbacks
    void loop();
    State* getCurrentState() { return currentState; }

private:
    State* currentState;
    volatile State* requested_state = nullptr; // New pointer for requested state
    AppContext& context;
};

#endif // STATE_MANAGER_H
