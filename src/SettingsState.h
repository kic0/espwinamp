#ifndef SETTINGS_STATE_H
#define SETTINGS_STATE_H

#include "State.h"
#include <ESPAsyncWebServer.h>
#include <Arduino.h>

class SettingsState : public State {
public:
    SettingsState() : server(80) {}
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;
    StateType getType() const override { return StateType::SETTINGS; }

private:
    AsyncWebServer server;
    bool wifi_ap_enabled = false;
    String wifi_ssid;
    String wifi_password;
    int selected_setting = 0;

    void draw_settings_ui(AppContext& context);
    State* handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button);
};

#endif // SETTINGS_STATE_H
