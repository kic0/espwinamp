#ifndef SETTINGS_STATE_H
#define SETTINGS_STATE_H

#include "State.h"
#include <ESPAsyncWebServer.h>
#include <Arduino.h>

void wifiTask(void* parameter);

class SettingsState : public State {
public:
    friend void wifiTask(void* parameter);
    SettingsState();
    void enter(AppContext& context) override;
    State* loop(AppContext& context) override;
    void exit(AppContext& context) override;
    StateType getType() const override { return StateType::SETTINGS; }

    // Public getters for UI
    bool is_ap_enabled() const { return wifi_ap_enabled; }
    String get_ssid() const { return wifi_ssid; }
    String get_password() const { return wifi_password; }
    String get_ip_address() const { return WiFi.softAPIP().toString(); }
    int get_selected_item() const { return selected_setting; }
    void setup_web_server();


private:
    static AsyncWebServer* server;
    bool wifi_ap_enabled = false;
    String wifi_ssid;
    String wifi_password;
    int selected_setting = 0;

    State* handle_button_press(AppContext& context, bool is_short_press);
};

#endif // SETTINGS_STATE_H
