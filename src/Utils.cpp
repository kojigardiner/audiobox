#include "Utils.h"

#include <WiFi.h>

#include "Constants.h"
#include "base64.h"

// Make sure all prefs with need to launch are present
bool check_prefs() {
    bool ret = true;
    Preferences prefs;
    char wifi_ssid[CLI_MAX_CHARS];
    char wifi_pass[CLI_MAX_CHARS];
    char spotify_client_id[CLI_MAX_CHARS];
    char spotify_auth_b64[CLI_MAX_CHARS];

    prefs.begin(APP_NAME, true);
    if (!(prefs.getString(PREFS_WIFI_SSID_KEY, wifi_ssid, CLI_MAX_CHARS) &&
          prefs.getString(PREFS_WIFI_PASS_KEY, wifi_pass, CLI_MAX_CHARS) &&
          prefs.getString(PREFS_SPOTIFY_CLIENT_ID_KEY, spotify_client_id, CLI_MAX_CHARS) &&
          prefs.getString(PREFS_SPOTIFY_AUTH_B64_KEY, spotify_auth_b64, CLI_MAX_CHARS))) {
        ret = false;
    }
    prefs.end();

    return ret;
}

// Connect to wifi network
void connect_wifi() {
    if (WiFi.status() != WL_CONNECTED) {
        // get wifi network info from prefs

        Preferences prefs;
        char wifi_ssid[CLI_MAX_CHARS];
        char wifi_pass[CLI_MAX_CHARS];

        prefs.begin(APP_NAME, true);
        if (!prefs.getString(PREFS_WIFI_SSID_KEY, wifi_ssid, CLI_MAX_CHARS) || !prefs.getString(PREFS_WIFI_PASS_KEY, wifi_pass, CLI_MAX_CHARS)) {
            Serial.println("Failed to get wifi preferences!");
        }
        prefs.end();

        // WiFi Setup
        Serial.print("Wifi connecting to ");
        Serial.println(wifi_ssid);
        WiFi.mode(WIFI_STA);
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
        WiFi.setHostname(APP_NAME);
        WiFi.begin(wifi_ssid, wifi_pass);

        bool led_state = HIGH;
        while (WiFi.status() != WL_CONNECTED) {
            digitalWrite(PIN_BUTTON_LED, led_state);  // blink the LED
            led_state = !led_state;
            Serial.print(".");
            delay(500);
        }
    }
    Serial.print("connected\n");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
}

// Set a value in preference. Pass an optional "value" key to set that value, otherwise prompt the user
void set_pref(Preferences *prefs, const char *key, const char *value) {
    char rcvd[CLI_MAX_CHARS];
    int stored;

    if (value == NULL) {
        Serial.print("Enter new value for ");
        Serial.print(key);
        Serial.println(": ");

        get_input(rcvd);
        Serial.print("Storing: ");
        Serial.println(rcvd);
        stored = prefs->putString(key, rcvd);
    } else {
        Serial.print("For key: ");
        Serial.println(key);

        Serial.print("Storing: ");
        Serial.println(value);
        stored = prefs->putString(key, value);
    }
    delay(1000);

    if (stored > 0) {
        // Serial.print("input length: ");
        // Serial.println(len);
        // Serial.print("bytes stored: ");
        // Serial.println(stored);
    } else {
        Serial.println("Failed to store to preferences");
    }
}

// Return a base64 encoded username:password
String compute_auth_b64(String user, String pass) {
    return base64::encode(user + ":" + pass);
}

// Prompt user for CLI input
int get_input(char *rcvd) {
    int idx = 0;
    char c;

    memset(rcvd, 0, sizeof(char) * CLI_MAX_CHARS);  // reset

    while (true) {
        if (Serial.available()) {
            c = Serial.read();
            Serial.print(c);  // echo

            if (c != '\r') {           // ignore carriage return
                if (c == '\n') {       // quit on line feed (new line) or max length
                    rcvd[idx] = '\0';  // null terminate
                    return idx;
                } else if (idx == CLI_MAX_CHARS - 1) {
                    Serial.println("WARNING: value entered is longer than maximum number of characters allowed!");
                    rcvd[idx] = '\0';  // null terminate
                    return idx;
                } else {
                    rcvd[idx] = c;
                    idx++;
                }
            }
        }
    }
}