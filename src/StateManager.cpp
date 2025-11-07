#include "StateManager.h"
#include "State.h"

StateManager* g_stateManager = nullptr;

StateManager::StateManager(AppContext& ctx) : context(ctx), currentState(nullptr) {
    g_stateManager = this;
}

void StateManager::setState(State* newState) {
    if (currentState) {
        currentState->exit(context);
        delete currentState;
    }
    currentState = newState;
    context.ui_dirty = true;
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
