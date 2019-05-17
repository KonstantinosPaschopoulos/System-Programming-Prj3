#include <stdint.h>

#ifndef MYTYPES_H
#define MYTYPES_H

// Maximum number of clients the select function will monitor
#define MAX_CLIENTS 45

typedef struct connected_node {
  // Storing the IP and port in binary form
  uint32_t clientIP;
  uint16_t clientPort;
  struct connected_node *next;
} connected_node;

typedef struct connected_list {
  connected_node *nodes;
} connected_list;

#endif
