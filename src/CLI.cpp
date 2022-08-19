#include "CLI.h"

#include <Arduino.h>
#include <Preferences.h>

#include "CLIMenu.h"
#include "Constants.h"
#include "Spotify.h"
#include "Utils.h"

void start_cli() {
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
        CLIMenuItem("Setup new wifi SSID (scan)", &set_wifi_prefs_scan),
        CLIMenuItem("Setup new wifi SSID (manual)", &set_wifi_prefs),
        CLIMenuItem("Test wifi connection", &test_wifi_connection),
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

    char wifi_ssid[CLI_MAX_CHARS];

    print("Current wifi network: ");
    if (!prefs.getString(PREFS_WIFI_SSID_KEY, wifi_ssid, CLI_MAX_CHARS)) {
        print("*** not set ***\n");
    } else {
        print("%s\n", wifi_ssid);
    }
    prefs.end();
}

void set_wifi_prefs() {
    Preferences prefs;
    prefs.begin(APP_NAME, false);
    set_pref(&prefs, PREFS_WIFI_SSID_KEY);
    set_pref(&prefs, PREFS_WIFI_PASS_KEY);
    prefs.end();
}

void set_wifi_prefs_scan() {
    print("\nScanning wifi networks...\n");
    int n = WiFi.scanNetworks();
    if (n == 0) {
        print("No networks found, try using manual method to set wifi SSID.\n");
    } else {
        print("%d networks found:\n", n);
        for (int i = 0; i < n; i++) {
            print("\t%d. %s (%d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
    }
    print("\t0. Return\n");

    char rcvd[CLI_MAX_CHARS];
    int idx = -1;
    while (idx < 0 || idx > n) {
        print("Select a network: ");
        get_input(rcvd);
        idx = atoi(rcvd);
        if (idx == 0) return;
    }
    print("%s selected.\nEnter pass: ", WiFi.SSID(idx - 1).c_str());
    get_input(rcvd);

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    set_pref(&prefs, PREFS_WIFI_SSID_KEY, WiFi.SSID(idx - 1).c_str());
    set_pref(&prefs, PREFS_WIFI_PASS_KEY, rcvd);
    prefs.end();
}

void set_spotify_client_id() {
    char client_id[CLI_MAX_CHARS];
    char client_secret[CLI_MAX_CHARS];
    char auth_b64[CLI_MAX_CHARS];
    int stored;

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    set_pref(&prefs, PREFS_SPOTIFY_CLIENT_ID_KEY);
    set_pref(&prefs, PREFS_SPOTIFY_CLIENT_SECRET_KEY);

    if (!prefs.getString(PREFS_SPOTIFY_CLIENT_ID_KEY, client_id, CLI_MAX_CHARS) ||
        !prefs.getString(PREFS_SPOTIFY_CLIENT_SECRET_KEY, client_secret, CLI_MAX_CHARS)) {
        print("Failed to retrieve Spotify credentials\n");
        return;
    }

    compute_auth_b64(client_id, client_secret, auth_b64);
    print("Storing: %s\n", auth_b64);
    stored = prefs.putString(PREFS_SPOTIFY_AUTH_B64_KEY, auth_b64);
    if (stored > 0) {
        // Serial.print("input length: ");
        // Serial.println(len);
        // Serial.print("bytes stored: ");
        // Serial.println(stored);
    } else {
        print("Failed to store to preferences\n");
    }
    prefs.end();
}

bool get_spotify_auth_url(char *auth_url) {
    char client_id[CLI_MAX_CHARS];
    char auth_b64[CLI_MAX_CHARS];

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    if (!prefs.getString(PREFS_SPOTIFY_CLIENT_ID_KEY, client_id, CLI_MAX_CHARS) ||
        !prefs.getString(PREFS_SPOTIFY_AUTH_B64_KEY, auth_b64, CLI_MAX_CHARS)) {
        print("Set Spotify client ID first!\n");
        prefs.end();
        return false;
    }

    if (!connect_wifi()) {
        prefs.end();
        return false;
    }

    prefs.end();
    return Spotify::request_user_auth(client_id, auth_b64, auth_url);
}

bool get_spotify_refresh_token(const char *auth_code, char *refresh_token) {
    char auth_b64[CLI_MAX_CHARS];

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    if (!prefs.getString(PREFS_SPOTIFY_AUTH_B64_KEY, auth_b64, CLI_MAX_CHARS)) {
        print("Set Spotify client ID first!\n");
        prefs.end();
        return false;
    }

    if (!connect_wifi()) {
        prefs.end();
        return false;
    }

    prefs.end();
    return Spotify::get_refresh_token(auth_b64, auth_code, refresh_token);
}

void save_spotify_refresh_token(char *refresh_token) {
    Preferences prefs;
    prefs.begin(APP_NAME, false);
    set_pref(&prefs, PREFS_SPOTIFY_REFRESH_TOKEN_KEY, refresh_token);
    prefs.end();
}

void set_spotify_account() {
    char refresh_token[CLI_MAX_CHARS];
    char auth_url[HTTP_MAX_CHARS];
    char auth_code[CLI_MAX_CHARS];

    if (get_spotify_auth_url(auth_url)) {
        print("\nLaunch the following URL in your browser to authorize access: ");
        print("%s\n", auth_url);
        print("\nAfter granting access, you will be redirected to a webpage.\n");

        // get the authorization code from the user
        print("\nCopy and paste the characters after \"code: \" on the webpage. Do not include the quotation (\") marks: ");
        get_input(auth_code);

        if (get_spotify_refresh_token(auth_code, refresh_token)) {
            save_spotify_refresh_token(refresh_token);
        }
    } else {
        print("Request user authorization failed\n");
    }
}

void clear_prefs() {
    Preferences prefs;
    prefs.begin(APP_NAME, false);
    print("Clearing preferences\n");
    prefs.clear();
    prefs.end();
}

void test_wifi_connection() {
    print("\n");
    connect_wifi();
    print("disconnecting\n");
    WiFi.disconnect();
}