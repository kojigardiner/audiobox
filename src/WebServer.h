#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

#include "EventHandler.h"

// This header and its associated WebServer.cpp file define functions that are used by the
// ESPWebServer to serve web pages to browser clients for controlling the Audiobox.

// Globals variables defined in main.cpp
extern AsyncWebServer server;
extern AsyncEventSource web_events;
extern EventHandler eh;
extern DNSServer dns_server;
extern bool ready_to_set_spotify_user;  // flag to main thread

// Server modes
enum server_mode_t {
    WEB_CONTROL,    // serve standard landing page for controlling Audiobox
    WEB_SETUP,      // serve setup specific landing page for wifi and Spotify account setup
    WEB_AP          // enable access-point mode for configuring wifi
};

// Initializes the web server for a given server mode and sets up all handlers
void start_web_server(server_mode_t server_mode);

// Async handlers for client requests
void handle_wifi_test(AsyncWebServerRequest* request);
void handle_exit(AsyncWebServerRequest* request);
void handle_wifi_manual(AsyncWebServerRequest* request);
void handle_spotify_client(AsyncWebServerRequest* request);
void handle_spotify_account(AsyncWebServerRequest* request);
void handle_spotify_auth(AsyncWebServerRequest* request);
void handle_change_mode(AsyncWebServerRequest* request);

#endif  // _WEBSERVER_H