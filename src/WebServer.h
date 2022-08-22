#ifndef WEB_SETUP_H_
#define WEB_SETUP_H_

#include <ESPAsyncWebServer.h>

#include "EventHandler.h"

// Below are globals defined in main.cpp
extern AsyncWebServer server;
extern AsyncEventSource web_events;
extern EventHandler eh;

void web_prefs(bool ap_mode);
void web_prefs_end();

void handle_wifi_test(AsyncWebServerRequest* request);
void handle_exit(AsyncWebServerRequest* request);
void handle_wifi_manual(AsyncWebServerRequest* request);
void handle_spotify_client(AsyncWebServerRequest* request);
void handle_spotify_account(AsyncWebServerRequest* request);
void handle_spotify_auth(AsyncWebServerRequest* request);
void handle_change_mode(AsyncWebServerRequest* request);

#endif  // WEB_SETUP_H_