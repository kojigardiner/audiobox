#ifndef WEB_SETUP_H_
#define WEB_SETUP_H_

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include "Constants.h"
#include "Utils.h"

void web_prefs() {
    // Filessystem setup
    print("Setting up filesystem\n");
    if (!SPIFFS.begin(true)) {
        print("An Error has occurred while mounting SPIFFS\n");
        return;
    }

    // Create AsyncWebServer object on port 80
    AsyncWebServer server(80);

    // Connect to Wi-Fi network with SSID and password
    print("Setting device to AP mode\n");

    // NULL sets an open Access Point
    WiFi.softAP(APP_NAME, NULL);

    IPAddress IP = WiFi.softAPIP();
    print("IP address: %s\n", IP.toString().c_str());

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest* request) {
        int params = request->params();
        for (int i = 0; i < params; i++) {
            AsyncWebParameter* p = request->getParam(i);
            if (p->isPost()) {
                // // HTTP POST ssid value
                // if (p->name() == PARAM_INPUT_1) {
                //     ssid = p->value().c_str();
                //     Serial.print("SSID set to: ");
                //     Serial.println(ssid);
                //     // Write file to save value
                //     writeFile(SPIFFS, ssidPath, ssid.c_str());
                // }
                // // HTTP POST pass value
                // if (p->name() == PARAM_INPUT_2) {
                //     pass = p->value().c_str();
                //     Serial.print("Password set to: ");
                //     Serial.println(pass);
                //     // Write file to save value
                //     writeFile(SPIFFS, passPath, pass.c_str());
                // }
                // // HTTP POST ip value
                // if (p->name() == PARAM_INPUT_3) {
                //     ip = p->value().c_str();
                //     Serial.print("IP Address set to: ");
                //     Serial.println(ip);
                //     // Write file to save value
                //     writeFile(SPIFFS, ipPath, ip.c_str());
                // }
                // // HTTP POST gateway value
                // if (p->name() == PARAM_INPUT_4) {
                //     gateway = p->value().c_str();
                //     Serial.print("Gateway set to: ");
                //     Serial.println(gateway);
                //     // Write file to save value
                //     writeFile(SPIFFS, gatewayPath, gateway.c_str());
                // }
                print("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }
        request->send(200, "text/plain", "Done. ESP will restart.");
        delay(3000);
        ESP.restart();
    });
    server.begin();
}

#endif  // WEB_SETUP_H_