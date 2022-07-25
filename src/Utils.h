#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>
#include <Preferences.h>

void connect_wifi();
bool check_prefs();
int get_input(char *rcvd);
void set_pref(Preferences *prefs, const char *key, const char *value = NULL);
void compute_auth_b64(const char *user, const char *pass, char *auth_b64);
void print(const char *format, ...);

#endif  // _UTILS_H