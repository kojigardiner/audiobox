#ifndef WEB_SETUP_H_
#define WEB_SETUP_H_

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include "Constants.h"
#include "Utils.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Template processing
String processor(const String& var) {
    if (var == "CURRENT_SSID") {
        Preferences prefs;
        prefs.begin(APP_NAME, false);

        char wifi_ssid[CLI_MAX_CHARS];

        if (!prefs.getString(PREFS_WIFI_SSID_KEY, wifi_ssid, CLI_MAX_CHARS)) {
            prefs.end();
            return F("*** not set ***");
        }
        prefs.end();
        return F(wifi_ssid);
    } else if (var == "WIFI_STATUS") {
        if (WiFi.status() == WL_CONNECTED)
            return F("Connected");
        else
            return F("Not connected");
    }

    return String();
}

// Clean up on exit
void web_prefs_end() {
    server.end();
    WiFi.disconnect();
    SPIFFS.end();
}

void handle_wifi_test(AsyncWebServerRequest* request) {
    connect_wifi(true);

    // reload the page so we can see the status update
    request->send(SPIFFS, "/wifi.html", String(), false, processor);
}

// Handler for /exit
void handle_exit(AsyncWebServerRequest* request) {
    web_prefs_end();
    print("Restarting...");
    ESP.restart();
}

// Handler for /wifi_manual
void handle_wifi_manual(AsyncWebServerRequest* request) {
    int n_params = request->params();

    char ssid[CLI_MAX_CHARS];
    char pass[CLI_MAX_CHARS];

    for (int i = 0; i < n_params; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
            if (p->name() == "ssid") {
                strncpy(ssid, p->value().c_str(), CLI_MAX_CHARS);
            }
            if (p->name() == "pass") {
                strncpy(pass, p->value().c_str(), CLI_MAX_CHARS);
            }
        }
    }

    // check that we got valid non-zero length entries
    if (strlen(ssid) && strlen(pass)) {
        WiFi.disconnect();  // disconnect in case we were previously connected to another network

        Preferences prefs;
        prefs.begin(APP_NAME, false);
        set_pref(&prefs, PREFS_WIFI_SSID_KEY, ssid);
        set_pref(&prefs, PREFS_WIFI_PASS_KEY, pass);
        prefs.end();
    }

    // Send user back to wifi page
    request->send(SPIFFS, "/wifi.html", String(), false, processor);
}

void web_prefs() {
    // Filessystem setup
    print("Setting up filesystem\n");
    if (!SPIFFS.begin(true)) {
        print("An Error has occurred while mounting SPIFFS\n");
        return;
    }

    // Connect to Wi-Fi network with SSID and password
    print("Setting device to AP mode\n");

    WiFi.mode(WIFI_AP_STA);  // set to ap/sta mode so that we can test WiFi STA access

    // Setup AP with no password
    WiFi.softAP(APP_NAME, NULL);
    print("IP address: %s\n", WiFi.softAPIP().toString().c_str());

    server.on("/exit", HTTP_GET, handle_exit);
    server.on("/wifi-manual", HTTP_POST, handle_wifi_manual);
    server.on("/wifi-test", HTTP_GET, handle_wifi_test);

    // Handle all other static page requests
    server.serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html")
        .setTemplateProcessor(processor);

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404);
    });

    server.begin();
}

#endif  // WEB_SETUP_H_