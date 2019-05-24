#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include "types.h"

void logon(int newsock, connected_list *connected_clients, int *fds){
  int i;
  uint16_t port_recv;
  uint32_t ip_recv;
  connected_node *curr_client = connected_clients->nodes;
  connected_node *new_client;
  char *msg;

  msg = (char*)calloc(13, sizeof(char));
  if (msg == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  strcpy(msg, "USER_ON");

  // Receiving the port and the IP
  read(newsock, &port_recv, sizeof(port_recv));
  port_recv = ntohs(port_recv);
  read(newsock, &ip_recv, sizeof(ip_recv));
  ip_recv = ntohl(ip_recv);

  // Making sure the client is not already connected
  while (curr_client != NULL)
  {
    if ((curr_client->clientIP == ip_recv) && (curr_client->clientPort == port_recv))
    {
      printf("The client from: %d %d, is already connected.\n", port_recv, ip_recv);
      return;
    }

    curr_client = curr_client->next;
  }

  printf("Client: %d %d, has logged on.\n", port_recv, ip_recv);

  //Creating a new client node
  new_client = (connected_node*)malloc(sizeof(connected_node));
  if (new_client == NULL)
  {
    perror("Malloc failed");
    exit(2);
  }
  new_client->clientIP = ip_recv;
  new_client->clientPort = port_recv;
  new_client->next = NULL;

  // Updating the list
  if (connected_clients->nodes == NULL)
  {
    connected_clients->nodes = new_client;
  }
  else
  {
    curr_client = connected_clients->nodes;
    while ((curr_client->next) != NULL)
    {
      curr_client = curr_client->next;
    }

    curr_client->next = new_client;
  }

  // Sending USER_ON messages to every connected client
  port_recv = htons(new_client->clientPort);
  ip_recv = htonl(new_client->clientIP);
  for (i = 0; i < MAX_CLIENTS; i++)
  {
    if ((fds[i] != 0) && (fds[i] != newsock))
    {
      // USER_ON port, ip
      write(fds[i], msg, 13);
      write(fds[i], &port_recv, sizeof(port_recv));
      write(fds[i], &ip_recv, sizeof(ip_recv));
    }
  }

  free(msg);
}

void getclients(int newsock, connected_list *connected_clients){
  uint16_t port_net;
  uint32_t ip_net;
  connected_node *curr_client = connected_clients->nodes;
  int count = 0;
  char *buffer, *count_str;

  // Counting how many clients are connected
  while (curr_client != NULL)
  {
    count++;
    curr_client = curr_client->next;
  }

  buffer = (char*)calloc(11, sizeof(char));
  if (buffer == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  count_str = (char*)calloc(12, sizeof(char));
  if (count_str == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }

  // Write CLIENT_LIST N
  strcpy(buffer, "CLIENT_LIST");
  write(newsock, buffer, 11);
  memset(buffer, 0, 11);

  sprintf(count_str, "%d", count);
  write(newsock, count_str, 12);

  // Sending the id of every connected client: port, ip
  curr_client = connected_clients->nodes;
  while (curr_client != NULL)
  {
    port_net = htons(curr_client->clientPort);
    write(newsock, &port_net, sizeof(port_net));
    ip_net = htonl(curr_client->clientIP);
    write(newsock, &ip_net, sizeof(ip_net));

    curr_client = curr_client->next;
  }

  free(buffer);
  free(count_str);
}

void logoff(int newsock, connected_list *connected_clients, int *fds){
  connected_node ** nodes_ptr = &(connected_clients->nodes);
  connected_node *tmp;
  char *msg;
  uint16_t port_recv, port;
  uint32_t ip_recv, ip;
  int i;

  msg = (char*)calloc(13, sizeof(char));
  if (msg == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  strcpy(msg, "USER_OFF");

  // Receiving the IP and the port
  read(newsock, &port_recv, sizeof(port_recv));
  port_recv = ntohs(port_recv);
  read(newsock, &ip_recv, sizeof(ip_recv));
  ip_recv = ntohl(ip_recv);

  while (*nodes_ptr != NULL)
  {
    if (((**nodes_ptr).clientIP == ip_recv) && ((**nodes_ptr).clientPort == port_recv))
    {
      port = (**nodes_ptr).clientPort;
      ip = (**nodes_ptr).clientIP;
      tmp = *nodes_ptr;
      *nodes_ptr = tmp->next;
      free(tmp);
    }
    else
    {
      nodes_ptr = &((**nodes_ptr).next);
    }
  }

  printf("Client: %d %d, has logged off.\n", port_recv, ip_recv);

  // Sending USER_OFF messages to every connected client
  port_recv = htons(port);
  ip_recv = htonl(ip);
  for (i = 0; i < MAX_CLIENTS; i++)
  {
    if ((fds[i] != 0) && (fds[i] != newsock))
    {
      // USER_OFF port, ip
      write(fds[i], msg, 13);
      write(fds[i], &port_recv, sizeof(port_recv));
      write(fds[i], &ip_recv, sizeof(ip_recv));
    }
  }

  free(msg);
}
