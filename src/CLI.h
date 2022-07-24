#ifndef CLI_H
#define CLI_H

void setup_cli();
void cli_main();
void cli_wifi();
void cli_spotify();
void cli_clear_prefs();

void check_wifi_prefs();
void set_wifi_prefs();
void set_spotify_client_id();
void set_spotify_account();
void clear_prefs();

#endif  // CLI_H