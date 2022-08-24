#ifndef WEBSERVER_H_
#define WEBSERVER_H_

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

#include "EventHandler.h"

// Below are globals defined in main.cpp
extern AsyncWebServer server;
extern AsyncEventSource web_events;
extern EventHandler eh;
extern DNSServer dns_server;
extern bool ready_to_set_spotify_user;  // flag to main thread

enum server_mode_t {
    WEB_CONTROL,
    WEB_SETUP,
    WEB_AP
};

void start_web_server(server_mode_t server_mode);
void handle_wifi_test(AsyncWebServerRequest* request);
void handle_exit(AsyncWebServerRequest* request);
void handle_wifi_manual(AsyncWebServerRequest* request);
void handle_spotify_client(AsyncWebServerRequest* request);
void handle_spotify_account(AsyncWebServerRequest* request);
void handle_spotify_auth(AsyncWebServerRequest* request);
void handle_change_mode(AsyncWebServerRequest* request);

#endif  // WEBSERVER_H_