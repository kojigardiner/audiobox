#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>
#include <Preferences.h>

bool connect_wifi(bool async = false);
bool check_prefs();
int get_input(char *rcvd);
void set_pref(Preferences *prefs, const char *key, const char *value = NULL);
void compute_auth_b64(const char *user, const char *pass, char *auth_b64);
void print(const char *format, ...);

void get_memory_stats_with_caller(char const *caller_name, int line);

#define get_memory_stats() get_memory_stats_with_caller(__func__, __LINE__)  // macro

#endif  // _UTILS_H