#include "SettingsState.h"
#include "AppContext.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <SD.h>
#include "pins.h"
#include "ArtistSelectionState.h"

void SettingsState::enter(AppContext& context) {
    // Read WiFi credentials
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
        Serial.println("Could not find wifi_credentials.txt");
        wifi_ssid = "winampesp";
        wifi_password = "12345678";
    }

    if (SPIFFS.exists("/wifi_mode.txt")) {
        wifi_ap_enabled = true;

        Serial.println("Starting WiFi AP...");
        if (WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str())) {
            Serial.print("AP Started. IP Address: ");
            Serial.println(WiFi.softAPIP());
        } else {
            Serial.println("Failed to start WiFi AP.");
        }

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(SPIFFS, "/index.html", "text/html");
        });

        server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
            String path = "/";
            if (request->hasParam("path")) {
                path = request->getParam("path")->value();
            }
            String json = "[";
            File root = SD.open(path);
            File file = root.openNextFile();
            bool first = true;
            while(file){
                if (!first) {
                    json += ",";
                }
                first = false;
                json += "{\"name\":\"" + String(file.name()) + "\",\"isDirectory\":" + (file.isDirectory() ? "true" : "false") + ",\"size\":\"" + String(file.size()) + "\"}";
                file = root.openNextFile();
            }
            root.close();
            json += "]";
            request->send(200, "application/json", json);
        });

        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "File uploaded successfully");
        }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            if(!index){
                request->_tempFile = SD.open("/" + filename, "w");
            }
            if(len){
                request->_tempFile.write(data, len);
            }
            if(final){
                request->_tempFile.close();
            }
        });

        server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request){
            if(request->hasParam("path") && request->hasParam("isDir")){
                String path = request->getParam("path")->value();
                bool isDir = request->getParam("isDir")->value() == "true";
                if(isDir){
                    if(SD.rmdir(path)){
                        request->send(200, "text/plain", "Directory deleted");
                    } else {
                        request->send(500, "text/plain", "Delete failed");
                    }
                } else {
                    if(SD.remove(path)){
                        request->send(200, "text/plain", "File deleted");
                    } else {
                        request->send(500, "text/plain", "Delete failed");
                    }
                }
            } else {
                request->send(400, "text/plain", "Bad Request");
            }
        });

        server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
            if(request->hasParam("path")){
                String path = request->getParam("path")->value();
                request->send(SD, path, "application/octet-stream");
            } else {
                request->send(400, "text/plain", "Bad Request");
            }
        });

        server.begin();
    }
}

State* SettingsState::loop(AppContext& context) {
    // Button handling
    bool current_scroll = !digitalRead(BTN_SCROLL);
    static bool scroll_pressed = false;
    static unsigned long scroll_press_time = 0;
    static bool scroll_long_press_triggered = false;

    if (current_scroll && !scroll_pressed) {
        scroll_pressed = true;
        scroll_press_time = millis();
        scroll_long_press_triggered = false;
    } else if (!current_scroll && scroll_pressed) {
        scroll_pressed = false;
        if (!scroll_long_press_triggered) {
            return handle_button_press(context, true, true);
        }
    }
    if (scroll_pressed && !scroll_long_press_triggered && (millis() - scroll_press_time >= 1000)) {
        scroll_long_press_triggered = true;
        return handle_button_press(context, false, true);
    }

    draw_settings_ui(context);
    return nullptr;
}

void SettingsState::exit(AppContext& context) {
    server.end();
}

State* SettingsState::handle_button_press(AppContext& context, bool is_short_press, bool is_scroll_button) {
    if (is_scroll_button && is_short_press) {
        if (!wifi_ap_enabled) {
            selected_setting = (selected_setting + 1) % 2;
            context.ui_dirty = true;
        }
    } else if (is_scroll_button && !is_short_press) {
        if (selected_setting == 0) {
            if (wifi_ap_enabled) {
                SPIFFS.remove("/wifi_mode.txt");
            } else {
                File file = SPIFFS.open("/wifi_mode.txt", "w");
                file.close();
            }
            ESP.restart();
        } else {
            return new ArtistSelectionState();
        }
    }
    return nullptr;
}

void SettingsState::draw_settings_ui(AppContext& context) {
    if (!context.ui_dirty) return;
    context.ui_dirty = false;
    context.display.clearDisplay();
    context.display.setTextSize(1);
    context.display.setTextColor(SSD1306_WHITE);

    context.display.setCursor(0, 0);
    context.display.print("Settings");
    context.display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // WiFi AP option
    context.display.setCursor(0, 26);
    String wifi_status = wifi_ap_enabled ? "on" : "off";
    if (selected_setting == 0) {
        context.display.print("> WiFi AP: [" + wifi_status + "]");
    } else {
        context.display.print("  WiFi AP: [" + wifi_status + "]");
    }

    if (wifi_ap_enabled) {
        context.display.setCursor(0, 36);
        context.display.print(wifi_ssid);
        context.display.setCursor(0, 46);
        context.display.print(wifi_password);
    } else {
        // Back option
        context.display.setCursor(0, 36);
        if (selected_setting == 1) {
            context.display.print("> <- back");
        } else {
            context.display.print("  <- back");
        }
    }

    context.display.display();
}
