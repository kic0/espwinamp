#include "SettingsState.h"
#include "AppContext.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <SD.h>
#include "pins.h"
#include "ArtistSelectionState.h"
#include "Log.h"
#include "UI.h"

void stop_audio_playback(AppContext& context); // Forward declaration from Audio.cpp

void SettingsState::enter(AppContext& context) {
    Log::printf("Entering Settings State\n");

    File wifi_file = SPIFFS.open("/wifi_credentials.txt", "r");
    if (wifi_file) {
        wifi_ssid = wifi_file.readStringUntil('\n');
        wifi_password = wifi_file.readStringUntil('\n');
        wifi_ssid.replace("SSID: ", "");
        wifi_password.replace("password: ", "");
        wifi_ssid.trim();
        wifi_password.trim();
        wifi_file.close();
    } else {
        Log::printf("Could not find wifi_credentials.txt, using defaults\n");
        wifi_ssid = "winampesp";
        wifi_password = "12345678";
    }

    if (SPIFFS.exists("/wifi_mode.txt")) {
        wifi_ap_enabled = true;
        setup_web_server(); // Start the server
    } else {
        wifi_ap_enabled = false;
    }
}

State* SettingsState::loop(AppContext& context) {
    ButtonPress press = context.button.read();
    if (press == SHORT_PRESS) {
        return handle_button_press(context, true);
    } else if (press == LONG_PRESS) {
        return handle_button_press(context, false);
    }

    draw_settings_ui(context);
    return nullptr;
}

void SettingsState::exit(AppContext& context) {
    server.end();
}

State* SettingsState::handle_button_press(AppContext& context, bool is_short_press) {
    int num_options = wifi_ap_enabled ? 4 : 2; // "Disable", "SSID", "Pass", "Back" OR "Enable", "Back"

    if (is_short_press) {
        selected_setting = (selected_setting + 1) % num_options;
        context.ui_dirty = true;
    } else { // Long press
        if (wifi_ap_enabled) {
            if (selected_setting == 0) { // Disable AP
                SPIFFS.remove("/wifi_mode.txt");
                ESP.restart();
            } else if (selected_setting == 3) { // Back
                return new ArtistSelectionState();
            }
        } else {
            if (selected_setting == 0) { // Enable AP
                stop_audio_playback(context);
                File file = SPIFFS.open("/wifi_mode.txt", "w");
                file.close();
                ESP.restart();
            } else if (selected_setting == 1) { // Back
                return new ArtistSelectionState();
            }
        }
    }
    return nullptr;
}

void SettingsState::setup_web_server() {
    Log::printf("Starting WiFi AP...\n");
    if (WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str())) {
        Log::printf("AP Started. IP Address: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Log::printf("Failed to start WiFi AP.\n");
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    // All other server endpoints remain the same...
    // To save space, they are omitted here but would be included in the real file.
    server.begin();
}
