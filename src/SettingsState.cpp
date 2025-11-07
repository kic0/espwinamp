#include "SettingsState.h"
#include "AppContext.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <SD.h>
#include "pins.h"
#include "ArtistSelectionState.h"
#include "Log.h"
#include "UI.h"

AsyncWebServer* SettingsState::server = nullptr;
extern AppContext* g_appContext;

void stop_audio_playback(AppContext& context);

void wifiTask(void* parameter) {
    AppContext* context = g_appContext;
    SettingsState* settingsState = (SettingsState*)parameter;

    settingsState->setup_web_server();

    while (!context->wifi_task_should_stop) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    if (SettingsState::server) {
        SettingsState::server->end();
        delete SettingsState::server;
        SettingsState::server = nullptr;
    }
    context->wifiTaskHandle = NULL;
    vTaskDelete(NULL);
}

SettingsState::SettingsState() {
    // Constructor remains empty, server is created on demand
}

void SettingsState::enter(AppContext& context) {
    Log::printf("Entering Settings State\n");
    vTaskSuspend(context.audioTaskHandle);

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
        context.wifi_task_should_stop = false;

        if (server == nullptr) {
            server = new AsyncWebServer(80);
        }

        xTaskCreatePinnedToCore(
            wifiTask, "WiFiTask", 8192, this, 1, &context.wifiTaskHandle, 1
        );
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
    return nullptr;
}

void SettingsState::exit(AppContext& context) {
    if (context.wifiTaskHandle != NULL) {
        context.wifi_task_should_stop = true;
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    vTaskResume(context.audioTaskHandle);
    Log::printf("Exiting Settings State\n");
}

State* SettingsState::handle_button_press(AppContext& context, bool is_short_press) {
    int num_options = wifi_ap_enabled ? 4 : 2;

    if (is_short_press) {
        selected_setting = (selected_setting + 1) % num_options;
        context.ui_dirty = true;
    } else {
        if (wifi_ap_enabled) {
            if (selected_setting == 0) {
                SPIFFS.remove("/wifi_mode.txt");
                ESP.restart();
            } else if (selected_setting == 3) {
                return new ArtistSelectionState();
            }
        } else {
            if (selected_setting == 0) {
                stop_audio_playback(context);
                File file = SPIFFS.open("/wifi_mode.txt", "w");
                file.close();
                ESP.restart();
            } else if (selected_setting == 1) {
                return new ArtistSelectionState();
            }
        }
    }
    return nullptr;
}

void SettingsState::setup_web_server() {
    Log::printf("Starting WiFi AP on Core 1...\n");
    WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str());
    Log::printf("AP Started. IP Address: %s\n", WiFi.softAPIP().toString().c_str());

    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server->begin();
}
