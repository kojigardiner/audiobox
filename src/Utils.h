#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>
#include <Preferences.h>

void connect_wifi();
bool check_prefs();
int get_input(char *rcvd);
void set_pref(Preferences *prefs, const char *key, const char *value = NULL);
String compute_auth_b64(String user, String pass);

#endif  // _UTILS_H