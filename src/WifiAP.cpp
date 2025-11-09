/*
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "esp_wifi.h"
#include <SPIFFS.h>
#include <SD.h>
#include "BluetoothA2DPSource.h"

// Forward declarations
extern BluetoothA2DPSource a2dp;
extern bool wifi_ap_enabled;
extern bool ui_dirty;

AsyncWebServer server(80);
String wifi_ssid;
String wifi_password;

String current_path = "/";

String processor(const String& var){
    if(var == "PATH"){
        return current_path;
    }
    if(var == "SD_INFO"){
        uint64_t total_bytes = SD.cardSize();
        uint64_t used_bytes = SD.usedBytes();
        return "SD Card: " + String((float)used_bytes / 1024 / 1024, 2) + " MB used / " + String((float)total_bytes / 1024 / 1024, 2) + " MB total";
    }
    if(var == "FILE_LIST"){
        String file_list = "";
        if (current_path != "/") {
            String parent_path = current_path.substring(0, current_path.lastIndexOf('/'));
            if (parent_path == "") parent_path = "/";
            file_list += "<tr><td><a href='/?path=" + parent_path + "'>..</a></td><td></td><td></td></tr>";
        }

        File root = SD.open(current_path);
        File file = root.openNextFile();
        while(file){
            String file_name = String(file.name());
            String file_path = current_path == "/" ? "/" + file_name : current_path + "/" + file_name;

            if (file.isDirectory()) {
                file_list += "<tr><td><a href='/?path=" + file_path + "'>" + file_name + "/</a></td><td>-</td><td class='action-links'><a href='/delete?file=" + file_path + "'>Delete</a></td></tr>";
            } else {
                file_list += "<tr><td>" + file_name + "</td><td>" + String(file.size()) + "</td><td class='action-links'><a href='/delete?file=" + file_path + "'>Delete</a></td></tr>";
            }
            file = root.openNextFile();
        }
        root.close();
        return file_list;
    }
    return String();
}

void start_wifi_ap() {
    // Stop Bluetooth
    a2dp.end(true);
    delay(1000);
    esp_bt_controller_deinit();

    // Start WiFi
    File file = SPIFFS.open("/wifi_credentials.txt", "r");
    if (file) {
        String ssid_line = file.readStringUntil('\n');
        String pass_line = file.readStringUntil('\n');
        file.close();

        wifi_ssid = ssid_line.substring(ssid_line.indexOf('=') + 1);
        wifi_password = pass_line.substring(pass_line.indexOf('=') + 1);
        wifi_ssid.trim();
        wifi_password.trim();

        WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str());

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            if (request->hasParam("path")) {
                current_path = request->getParam("path")->value();
            } else {
                current_path = "/";
            }
            request->send(SPIFFS, "/index.html", "text/html", false, processor);
        });

        server.on("/upload", HTTP_POST,
            [](AsyncWebServerRequest *request){
                String path = "/";
                if(request->hasParam("path", true)) {
                    path = request->getParam("path", true)->value();
                }
                request->redirect("/?path=" + path);
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
                String path = "/";
                if(request->hasParam("path", true)) {
                    path = request->getParam("path", true)->value();
                }

                if(!index){
                    String file_path;
                    if (path == "/") {
                        file_path = "/" + filename;
                    } else {
                        file_path = path + "/" + filename;
                    }
                    request->_tempFile = SD.open(file_path, FILE_WRITE);
                }
                if(len && request->_tempFile){
                    request->_tempFile.write(data, len);
                }
                if(final && request->_tempFile){
                    request->_tempFile.close();
                }
            }
        );

        server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
            String path = "/";
            if (request->hasParam("file")) {
                String file_path = request->getParam("file")->value();

                int last_slash = file_path.lastIndexOf('/');
                if (last_slash > 0) {
                    path = file_path.substring(0, last_slash);
                }

                if (SD.remove(file_path)) {
                    // file deleted
                } else if (SD.rmdir(file_path)) {
                    // directory deleted
                }
            }
            request->redirect("/?path=" + path);
        });

        server.on("/mkdir", HTTP_GET, [](AsyncWebServerRequest *request){
            String path = "/";
            if (request->hasParam("path")) {
                path = request->getParam("path")->value();
            }
            if (request->hasParam("dirname")) {
                String dirname = request->getParam("dirname")->value();
                String dir_path;
                if (path == "/") {
                    dir_path = "/" + dirname;
                } else {
                    dir_path = path + "/" + dirname;
                }
                SD.mkdir(dir_path);
            }
            request->redirect("/?path=" + path);
        });

        server.begin();
    }
    wifi_ap_enabled = true;
    ui_dirty = true;
}
*/
