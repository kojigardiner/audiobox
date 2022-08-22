#include "WebServer.h"

#include <SPIFFS.h>

#include "CLI.h"
#include "Constants.h"
#include "Utils.h"

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
            return F("Not Connected");
    } else if (var == "SPOTIFY_USER") {
        Preferences prefs;
        prefs.begin(APP_NAME, false);

        char sp_user[CLI_MAX_CHARS];

        if (!prefs.getString(PREFS_SPOTIFY_USER_NAME_KEY, sp_user, CLI_MAX_CHARS)) {
            prefs.end();
            return F("*** not set ***");
        }
        prefs.end();
        return F(sp_user);
    }

    return String();
}

void handle_wifi_test(AsyncWebServerRequest* request) {
    connect_wifi(true);

    // reload the page so we can see the status update
    request->send(SPIFFS, "/wifi.html", String(), false, processor);
}

// Handler for /exit
void handle_exit(AsyncWebServerRequest* request) {
    print("Rebooting...\n");
    event_t e = {.event_type = EVENT_REBOOT};
    eh.emit(e);
    request->send(SPIFFS, "/index.html", String(), false, processor);  // send user back home
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

// Handler for /spotify_client
void handle_spotify_client(AsyncWebServerRequest* request) {
    int n_params = request->params();

    char client_id[CLI_MAX_CHARS];
    char client_secret[CLI_MAX_CHARS];
    char auth_b64[CLI_MAX_CHARS];

    for (int i = 0; i < n_params; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
            if (p->name() == "client-id") {
                strncpy(client_id, p->value().c_str(), CLI_MAX_CHARS);
            }
            if (p->name() == "client-secret") {
                strncpy(client_secret, p->value().c_str(), CLI_MAX_CHARS);
            }
        }
    }

    Preferences prefs;
    prefs.begin(APP_NAME, false);
    // check that we got valid non-zero length entries
    if (strlen(client_id) && strlen(client_secret)) {
        set_pref(&prefs, PREFS_SPOTIFY_CLIENT_ID_KEY, client_id);
        set_pref(&prefs, PREFS_SPOTIFY_CLIENT_SECRET_KEY, client_secret);
    }

    compute_auth_b64(client_id, client_secret, auth_b64);
    print("Storing: %s\n", auth_b64);
    prefs.putString(PREFS_SPOTIFY_AUTH_B64_KEY, auth_b64);
    prefs.end();

    // Send user back to spotify page
    request->send(SPIFFS, "/spotify.html", String(), false, processor);
}
void handle_spotify_account(AsyncWebServerRequest* request) {
    char auth_url[HTTP_MAX_CHARS];
    get_spotify_auth_url(auth_url);

    print("%s\n", auth_url);
    // Send user to the authorization url
    AsyncWebServerResponse* response = request->beginResponse(303);  // HTTP SEE OTHER, forces browser to GET
    response->addHeader("Location", auth_url);
    request->send(response);
}

void handle_spotify_auth(AsyncWebServerRequest* request) {
    int n_params = request->params();

    for (int i = 0; i < n_params; i++) {
        AsyncWebParameter* p = request->getParam(i);

        if (p->name() == "code") {
            if (get_and_save_spotify_refresh_token(p->value().c_str())) {
                get_and_save_spotify_user_name();
            }
        }
    }

    // Send user back to spotify page
    request->send(SPIFFS, "/spotify.html", String(), false, processor);
}

void handle_change_mode(AsyncWebServerRequest* request) {
    button_event_t button_event = {.id = PIN_BUTTON_MODE, .state = ButtonFSM::MOMENTARY_TRIGGERED};
    event_t e = {.event_type = EVENT_BUTTON_PRESSED, {.button_info = button_event}};
    eh.emit(e);

    request->send(SPIFFS, "/index.html", String(), false, processor);
}

void web_prefs(bool ap_mode) {
    // Filessystem setup
    print("Setting up filesystem\n");
    if (!SPIFFS.begin(true)) {
        print("An Error has occurred while mounting SPIFFS\n");
        return;
    }

    connect_wifi();  // kick off a wifi connection with the existing config if possible

    if (ap_mode) {
        // Connect to Wi-Fi network with SSID and password
        print("Setting up AP mode\n");
        WiFi.mode(WIFI_AP_STA);  // set to ap/sta mode so that we can test WiFi STA access

        // Setup AP with no password
        WiFi.softAP(APP_NAME, NULL);
        print("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    }

    server.on("/exit", HTTP_GET, handle_exit);
    server.on("/wifi-manual", HTTP_POST, handle_wifi_manual);
    server.on("/wifi-test", HTTP_GET, handle_wifi_test);
    server.on("/spotify-client", HTTP_POST, handle_spotify_client);
    server.on("/spotify-account", HTTP_GET, handle_spotify_account);
    server.on("/spotify-auth", HTTP_GET, handle_spotify_auth);
    server.on("/change-mode", HTTP_GET, handle_change_mode);

    // Handle all other static page requests
    server.serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html")
        .setTemplateProcessor(processor);

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404);
    });

    // Handle events
    web_events.onConnect([](AsyncEventSourceClient* client) {
        if (client->lastId()) {
            print("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        // send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("Hello from the server!", NULL, millis(), 1000);
    });
    server.addHandler(&web_events);

    server.begin();
}
