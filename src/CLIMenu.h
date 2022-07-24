#ifndef _CLIMENU_H
#define _CLIMENU_H

#include "CLIMenuItem.h"

class CLIMenu {
   public:
    // Constructor
    CLIMenu() = default;
    CLIMenu(const char *text, const CLIMenuItem *items, int nitems, CLIMenu *prev = 0);

    // Display the menu
    void show();

    // Run the cli loop for the menu
    void run_cli();

    // Execute the command at index i in the menu list
    // return true if we should continue in this menu or false if we should exit
    bool run_command(int i);

   private:
    char _text[MAX_CLI_MENU_TEXT];           // text to display for this menu, ahead of the items in it
    CLIMenu *_prev;                          // pointer to a previous menu
    CLIMenuItem _items[MAX_CLI_MENU_ITEMS];  // list of menu items
    int _nitems;                             // number of items actually on the list
};

#endif  // _CLIMENU_H