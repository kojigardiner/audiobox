#include "Utils.h"

#include <WiFi.h>

#include "Constants.h"
#include "base64.h"
#include "stdarg.h"
#include "stdio.h"

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
// Passing an argument of async = true will cause the function to return immediately and not wait
// for connection to be established
bool connect_wifi(bool async) {
    unsigned long start_ms;

    if (WiFi.status() != WL_CONNECTED) {
        // get wifi network info from prefs

        Preferences prefs;
        char wifi_ssid[CLI_MAX_CHARS];
        char wifi_pass[CLI_MAX_CHARS];

        prefs.begin(APP_NAME, true);
        if (!prefs.getString(PREFS_WIFI_SSID_KEY, wifi_ssid, CLI_MAX_CHARS) || !prefs.getString(PREFS_WIFI_PASS_KEY, wifi_pass, CLI_MAX_CHARS)) {
            print("Failed to get wifi preferences!\n");
        }
        prefs.end();

        // WiFi Setup
        print("Wifi connecting to %s", wifi_ssid);

        // Only set a new mode if we are not already operating
        // This allows connect_wifi() to be called when in soft AP mode
        if (WiFi.getMode() == WIFI_MODE_NULL) WiFi.mode(WIFI_STA);

        // WiFi.config(IPAddress(WIFI_LOCAL_IP), IPAddress(WIFI_GATEWAY), IPAddress(WIFI_SUBNET), IPAddress(WIFI_DNS1), IPAddress(WIFI_DNS2));
        WiFi.config(IPADDR_NONE, IPADDR_NONE, IPADDR_NONE, IPADDR_NONE);
        WiFi.setHostname(APP_NAME);
        WiFi.begin(wifi_ssid, wifi_pass);

        bool led_state = HIGH;
        if (!async) {
            start_ms = millis();
            while (WiFi.status() != WL_CONNECTED) {
                digitalWrite(PIN_BUTTON_LED, led_state);  // blink the LED
                led_state = !led_state;
                Serial.print(".");
                delay(500);
                if (millis() - start_ms > WIFI_TIMEOUT_MS) {
                    print("connection timed out\n");
                    return false;
                }
            }
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        print("connected\n");
        print("IP: %s\n", WiFi.localIP().toString().c_str());
        print("RSSI: %d\n", WiFi.RSSI());
        return true;
    }
    return false;
}

// Set a value in preference. Pass an optional "value" key to set that value, otherwise prompt the user
void set_pref(Preferences *prefs, const char *key, const char *value) {
    char rcvd[CLI_MAX_CHARS];
    int stored;

    if (value == NULL) {
        print("Enter new value for %s: ", key);

        get_input(rcvd);
        print("Storing (length %d): %s\n", strlen(rcvd), rcvd);
        stored = prefs->putString(key, rcvd);
    } else {
        print("For key: %s\n", key);

        print("Storing (length %d): %s\n", strlen(value), value);
        stored = prefs->putString(key, value);
    }
    // delay(500);

    if (stored > 0) {
        // Serial.print("input length: ");
        // Serial.println(len);
        // Serial.print("bytes stored: ");
        // Serial.println(stored);
    } else {
        print("Failed to store to preferences\n");
    }
}

// Return a base64 encoded username:password
void compute_auth_b64(const char *user, const char *pass, char *auth_b64) {
    char combined[2 * CLI_MAX_CHARS + 1];
    snprintf(combined, CLI_MAX_CHARS, "%s:%s", user, pass);

    strncpy(auth_b64, base64::encode(combined).c_str(), CLI_MAX_CHARS);
}

// Prompt user for CLI input
int get_input(char *rcvd) {
    int idx = 0;
    char c;

    memset(rcvd, 0, sizeof(char) * CLI_MAX_CHARS);  // reset

    while (true) {
        if (Serial.available()) {
            c = Serial.read();
            print("%c", c);  // echo

            if (c != '\r') {           // ignore carriage return
                if (c == '\n') {       // quit on line feed (new line) or max length
                    rcvd[idx] = '\0';  // null terminate
                    return idx;
                } else if (idx == CLI_MAX_CHARS - 1) {
                    Serial.print("WARNING: value entered is longer than maximum number of characters allowed!\n");
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

void print(const char *format, ...) {
    static char buffer[HTTP_MAX_CHARS];

    va_list args;
    va_start(args, format);

    if (strlen(format) > (HTTP_MAX_CHARS - 2)) Serial.println("WARNING! Printed text will be truncated!");
    int ret = vsnprintf(buffer, HTTP_MAX_CHARS, format, args);
    if (ret < 0) Serial.println("WARNING! Print encoding failed!");
    Serial.print(buffer);

    va_end(args);
}

void get_memory_stats_with_caller(const char *caller_name, int line) {
    print("get_memory_stats() called from: %s:%d\n", caller_name, line);
    print("total free heap (MALLOC_CAP_INTERNAL):    %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    print("total free heap (MALLOC_CAP_DEFAULT):     %d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    print("largest free block (MALLOC_CAP_INTERNAL): %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    print("largest free block (MALLOC_CAP_DEFAULT):  %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    print("task high-water mark:                     %d\n", uxTaskGetStackHighWaterMark(NULL));
}
