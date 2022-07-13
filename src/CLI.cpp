#include "CLI.h"

int get_input(char *rcvd) {
    int idx = 0;
    char c;

    memset(rcvd, 0, sizeof(char) * MAX_CHARS);  // reset

    while (true) {
        if (Serial.available()) {
            c = Serial.read();
            Serial.print(c);  // echo

            if (c != '\r') {                                  // ignore carriage return
                if ((c == '\n') || (idx == MAX_CHARS - 1)) {  // quit on line feed (new line) or max length
                    rcvd[idx] = '\0';                         // null terminate
                    return idx;
                } else {
                    rcvd[idx] = c;
                    idx++;
                }
            }
        }
    }
}

void cli_main() {
    char rcvd[MAX_CHARS];
    int len;

    char menu[] =
        "\nSelect an option: \n"
        "\t1. Setup wifi\n"
        "\t2. Setup Spotify\n"
        "\t3. Clear preferences\n"
        "\t4. Quit\n";

    while (true) {
        Serial.println("CLI main menu");
        Serial.print(menu);

        len = get_input(rcvd);

        switch (rcvd[0]) {
            case '1':  // wifi
                cli_wifi();
                break;
            case '2':  // spotify
                cli_spotify();
                break;
            case '3':
                cli_clear_prefs();
                break;
            case '4':
            case 'q':
                Serial.println("Returning to program execution...");
                return;
            default:
                break;
        }
    }
}

void cli_wifi() {
    char rcvd[MAX_CHARS];
    char ssid[MAX_CHARS];
    char pass[MAX_CHARS];
    int len, stored;

    char menu[] =
        "\nSet new wifi SSID? \n"
        "\t1. Yes\n"
        "\t2. No\n";

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    while (true) {
        String wifi_ssid = prefs.getString(PREFS_WIFI_SSID_KEY, "");
        String wifi_pass = prefs.getString(PREFS_WIFI_PASS_KEY, "");

        Serial.print("Current wifi network: ");
        if (wifi_ssid == "") {
            Serial.println("*** not set ***");
        } else {
            Serial.println(wifi_ssid.c_str());
        }
        Serial.print(menu);

        len = get_input(rcvd);

        switch (rcvd[0]) {
            case '1':  // wifi
                Serial.println("Enter new network SSID: ");
                len = get_input(ssid);
                stored = prefs.putString(PREFS_WIFI_SSID_KEY, ssid);

                if (stored > 0) {
                    // Serial.print("input length: ");
                    // Serial.println(len);
                    // Serial.print("bytes stored: ");
                    // Serial.println(stored);
                } else {
                    Serial.println("Failed to store to preferences");
                }

                Serial.println("Enter password: ");
                len = get_input(pass);
                stored = prefs.putString(PREFS_WIFI_PASS_KEY, pass);

                if (stored > 0) {
                    // Serial.print("input length: ");
                    // Serial.println(len);
                    // Serial.print("bytes stored: ");
                    // Serial.println(stored);
                } else {
                    Serial.println("Failed to store to preferences");
                }
                prefs.end();
                return;
            case '2':
            case 'q':
                prefs.end();
                return;
            default:
                break;
        }
    }
}

void cli_spotify() {
    char rcvd[MAX_CHARS];
    int len;

    char menu[] =
        "\nSet new Spotify client ID? \n"
        "\t1. Yes\n"
        "\t2. No\n";

    while (true) {
        Serial.print("Current Spotify client ID: ");
        Serial.println(SPOTIFY_CLIENT_ID);
        Serial.print(menu);

        len = get_input(rcvd);

        switch (rcvd[0]) {
            case '1':  // change spotify credentials
                Serial.println("*** setup spotify ***");
                return;
            case '2':
            case 'q':
                return;
            default:
                break;
        }
    }
}

void cli_clear_prefs() {
    char rcvd[MAX_CHARS];
    int len;

    char menu[] =
        "\nAre you sure you want to clear all preferences? \n"
        "\t9. Yes\n"
        "\t2. No\n";

    Preferences prefs;
    prefs.begin(APP_NAME, false);

    while (true) {
        Serial.print(menu);

        len = get_input(rcvd);

        switch (rcvd[0]) {
            case '9':  // clear prefs
                Serial.println("Clearing preferences");
                prefs.clear();
                prefs.end();
                return;
            case '2':
            case 'q':
                prefs.end();
                return;
            default:
                break;
        }
    }
}