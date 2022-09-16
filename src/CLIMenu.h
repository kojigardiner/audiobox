#ifndef _CLIMENU_H
#define _CLIMENU_H

#include "CLIMenuItem.h"

// The CLIMenu class encapsulates a hierarchical command line menu with configurable 
// structure. A CLIMenu object consists of a string to be printed when that menu is active, 
// an array of CLIMenuItem items relevant for that node, and an optional pointer to
// a previous CLIMenu in the hierarchy.
class CLIMenu {
   public:
    // Default constructor, required in order to forward declare CLIMenu objects in order
    // to build the hierarchical structure (see CLI.cpp:start_cli()).
    CLIMenu() = default;

    // Takes a string with text to display for this menu, a list of menu items, the number of items, 
    // and an optional pointer to the previous menu.
    CLIMenu(const char *text, const CLIMenuItem *items, int nitems, CLIMenu *prev = 0);

    // Runs a while(true) loop, printing the menu contents, requesting user input, and acting on it.
    // Will not return until the user explicitly selects "exit" from the list of options.
    void run_cli();

   private:
    // Prints the CLIMenu text as well as a numbered list of the CLIMenuItems and their associated text.
    void _show();

    // Executes the command associated with the CLIMenuItem at index i in the menu list.
    // Returns true if we should continue to display the current menu, or false if we should exit.
    bool _run_command(int i);

    char _text[MAX_CLI_MENU_TEXT];           // text to display for this menu prior to printing the list of items
    CLIMenu *_prev;                          // pointer to a previous menu
    CLIMenuItem _items[MAX_CLI_MENU_ITEMS];  // list of menu items
    int _nitems;                             // number of items on the list
};

#endif  // _CLIMENU_H