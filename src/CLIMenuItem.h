#ifndef _CLIMENUITEM_H
#define _CLIMENUITEM_H

#include "Constants.h"

// Forward declaration
class CLIMenu;

// The CLIMenuItem class encapsulates a selectable menu item with a hierarchical command line
// interface system defined by CLIMenu. A CLIMenuItem consists of a text description of the
// selectable item, as well as function pointer to a function to execute when selected OR
// a pointer to another CLIMenu item (i.e. to move lower or higher within the menu hierarchy).
class CLIMenuItem {
   public:
    
    // Default constructor, required in order to forward declare CLIMenuItem objects in order
    // to build the hierarchical structure (see CLI.cpp:start_cli()).
    CLIMenuItem() = default;
    CLIMenuItem(const char *desc);                     // base constructor

    // The overloaded constructors below provide compile-time polymorphism for the two
    // tyeps of menu items: executable and menu-traversing.
    CLIMenuItem(const char *desc, void (*command)());  // basic function execution
    CLIMenuItem(const char *desc, CLIMenu *menu);      // new menu execution

    // Executes the action associated with the menu item, either jumping to a function pointer or 
    // jumping to a new menu level.
    void execute();

    // Returns a pointer to the description string.
    const char *get_desc();

   protected:
    char _desc[MAX_CLI_MENU_TEXT];  // description of the menu item to be printed
    void (*_command)();  // function pointer
    CLIMenu *_menu;      // pointer to a menu
};

#endif  // _CLIMENUITEM_H