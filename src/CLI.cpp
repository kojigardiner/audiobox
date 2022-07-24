#include "CLI.h"

#include <Arduino.h>
#include <Preferences.h>

#include "CLIMenu.h"
#include "Constants.h"
#include "Spotify.h"
#include "Utils.h"

void setup_cli() {
    CLIMenu menu_main, menu_wifi, menu_spotify, menu_clear_prefs;

    // Main
    const CLIMenuItem items_main[] = {
        CLIMenuItem("Setup wifi", &menu_wifi),
        CLIMenuItem("Setup Spotify", &menu_spotify),
        CLIMenuItem("Clear preferences", &menu_clear_prefs),
    };
    menu_main = CLIMenu("Preferences Menu", items_main, ARRAY_SIZE(items_main));

    // Wifi
    const CLIMenuItem items_wifi[] = {
        CLIMenuItem("Check current wifi SSID", &check_wifi_prefs),
        CLIMenuItem("Setup new wifi SSID", &set_wifi_prefs),
    };
    menu_wifi = CLIMenu("Wifi Menu", items_wifi, ARRAY_SIZE(items_wifi), &menu_main);

    // Spotify
    const CLIMenuItem items_spotify[] = {
        CLIMenuItem("Setup Spotify client ID", &set_spotify_client_id),
        CLIMenuItem("Setup Spotify account", &set_spotify_account),
    };
    menu_spotify = CLIMenu("Spotify Menu", items_spotify, ARRAY_SIZE(items_spotify), &menu_main);

    // Clear prefs
    const CLIMenuItem items_clear[] = {
        CLIMenuItem("Yes", &clear_prefs),
    };
    menu_clear_prefs = CLIMenu("Are you sure you want to clear all preferences?", items_clear, ARRAY_SIZE(items_clear), &menu_main);

    menu_main.run_cli();
}

void check_wifi_prefs() {
    Preferences prefs;
    prefs.begin(APP_NAME, false);

    String wifi_ssid = prefs.getString(PREFS_WIFI_SSID_KEY, "");
    prefs.end();

    Serial.print("Current wifi network: ");
    if (wifi_ssid == "") {
        Serial.println("*** not set ***");
    } else {
        Serial.println(wifi_ssid.c_str());
    }
}

void set_wifi_prefs() {
    Preferences prefs;
    prefs.begin(APP_NAME, false);
    set_pref(&prefs, PREFS_WIFI_SSID_KEY);
    set_pref(&prefs, PREFS_WIFI_PASS_KEY);
    prefs.end();
}

void set_spotify_client_id() {
    String auth_b64;
    int stored;

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    set_pref(&prefs, PREFS_SPOTIFY_CLIENT_ID_KEY);
    set_pref(&prefs, PREFS_SPOTIFY_CLIENT_SECRET_KEY);

    auth_b64 = compute_auth_b64(prefs.getString(PREFS_SPOTIFY_CLIENT_ID_KEY, ""), prefs.getString(PREFS_SPOTIFY_CLIENT_SECRET_KEY, ""));
    Serial.print("Storing: ");
    Serial.println(auth_b64);
    stored = prefs.putString(PREFS_SPOTIFY_AUTH_B64_KEY, auth_b64);
    if (stored > 0) {
        // Serial.print("input length: ");
        // Serial.println(len);
        // Serial.print("bytes stored: ");
        // Serial.println(stored);
    } else {
        Serial.println("Failed to store to preferences");
    }
    prefs.end();
}

void set_spotify_account() {
    char refresh_token[CLI_MAX_CHARS];

    connect_wifi();

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    if (Spotify::request_user_auth(
            prefs.getString(PREFS_SPOTIFY_CLIENT_ID_KEY, "").c_str(),
            prefs.getString(PREFS_SPOTIFY_AUTH_B64_KEY, "").c_str(),
            refresh_token)) {
        set_pref(&prefs, PREFS_SPOTIFY_REFRESH_TOKEN_KEY, refresh_token);
    } else {
        Serial.println("Request user authorization failed");
    }
    prefs.end();
}

void clear_prefs() {
    Preferences prefs;
    prefs.begin(APP_NAME, false);
    Serial.println("Clearing preferences");
    prefs.clear();
    prefs.end();
}
