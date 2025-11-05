#include "StateManager.h"
#include "State.h"

StateManager::StateManager(AppContext& ctx) : context(ctx), currentState(nullptr) {}

void StateManager::setState(State* newState) {
    if (currentState) {
        currentState->exit(context);
        delete currentState;
    }
    currentState = newState;
    if (currentState) {
        currentState->enter(context);
    }
}

void StateManager::loop() {
    if (currentState) {
        State* nextState = currentState->loop(context);
        if (nextState != nullptr) {
            setState(nextState);
        }
    }
}
