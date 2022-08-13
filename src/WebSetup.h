#ifndef WEB_SETUP_H_
#define WEB_SETUP_H_

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include "Constants.h"
#include "Utils.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

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
    }
    return String();
}

void web_prefs_end() {
    server.end();
    WiFi.disconnect();
    SPIFFS.end();
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

    // Setup AP with no password
    WiFi.softAP(APP_NAME, NULL);
    print("IP address: %s\n", WiFi.softAPIP().toString().c_str());

    // Handle exiting
    server.on("/exit", HTTP_GET, [](AsyncWebServerRequest* request) {
        web_prefs_end();
        print("Restarting...");
        ESP.restart();
    });

    // Handle user input of wifi credentials
    server.on("/wifi-manual", HTTP_POST, [](AsyncWebServerRequest* request) {
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
            Preferences prefs;
            prefs.begin(APP_NAME, false);
            set_pref(&prefs, PREFS_WIFI_SSID_KEY, ssid);
            set_pref(&prefs, PREFS_WIFI_PASS_KEY, pass);
            prefs.end();
        }

        // Send user back to wifi page
        request->send(SPIFFS, "/wifi.html", String(), false, processor);
    });

    // Handle all other static page requests
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setTemplateProcessor(processor);

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404);
    });

    // server.on("", HTTP_ANY, [](AsyncWebServerRequest* request) {
    //     print("Request: %s\n", request->url().c_str());
    //     request->send(SPIFFS, "/index.html", "text/html");
    //     // request->send(SPIFFS, "/index.htm", String(), false, processor);
    // });

    // server.on("/style.css", HTTP_ANY, [](AsyncWebServerRequest* request) {
    //     request->send(SPIFFS, "/style.css", "text/css");
    // });

    server.begin();
}

#endif  // WEB_SETUP_H_