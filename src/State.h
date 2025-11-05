#ifndef STATE_H
#define STATE_H

class AppContext; // Forward declaration

class State {
public:
    virtual ~State() {}
    virtual void enter(AppContext& context) = 0;
    virtual State* loop(AppContext& context) = 0; // Return new state or nullptr
    virtual void exit(AppContext& context) = 0;
};

#endif // STATE_H
