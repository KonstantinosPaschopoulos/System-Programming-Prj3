#include <pthread.h>
#include "types.h"

// Responds to a GET_FILE_LIST request
void getfilelist(int, char *);
// Recursively counts how many files are inside the dirName directory
int count_files(char *);
// Recursively sends every pathname inside the dirName directory
void pathnames(char *, int);
// Responds to a GET_FILE request
void getfile(int, char *);
// Creates all the directories that are needed for a path
void createfile(char *);
// Responds to the USER_OFF message
void useroff(int, connected_list *, pthread_mutex_t *);
// Returns the version of a file
uint32_t fileversion(char *);
