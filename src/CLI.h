#ifndef CLI_H
#define CLI_H

void start_cli();
void check_wifi_prefs();
void set_wifi_prefs();
void set_wifi_prefs_scan();
void set_spotify_client_id();
void set_spotify_account();
void clear_prefs();
void test_wifi_connection();
bool get_spotify_auth_url(char *auth_url);
bool get_spotify_refresh_token(const char *auth_code, char *refresh_token);
void save_spotify_refresh_token(char *refresh_token);

#endif  // CLI_H