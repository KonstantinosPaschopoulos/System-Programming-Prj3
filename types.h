#include <stdint.h>

#ifndef MYTYPES_H
#define MYTYPES_H

// Maximum number of file descriptors the select function will monitor
#define MAX_CLIENTS 45

typedef struct connected_node {
  uint32_t clientIP;
  uint16_t clientPort;
  struct connected_node *next;
} connected_node;

typedef struct connected_list {
  connected_node *nodes;
} connected_list;

typedef struct circular_node {
  char pathname[128];
  char version[128];
  uint16_t port;
  uint32_t ip;
} circular_node;

typedef struct pool_t {
  circular_node *buffer;
  int start;
  int end;
  int count;
  int size;
} pool_t;

// The arguments that get passed to the worker threads
typedef struct args {
  char mirrorDir[256];
  connected_list *list;
} args;

#endif
