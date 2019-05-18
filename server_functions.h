#include "types.h"

// Updates the list and sends the USER_ON messages
void logon(int, connected_list *);
// Counts the clients and sends their id
void getclients(int, connected_list *);
// Removes the client from the list and sends USER_OFF messages
void logoff(int, connected_list *);
