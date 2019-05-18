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

// Updates the list and sends the USER_ON messages
void logon(int newsock, connected_list *connected_clients){
  uint16_t port_recv;
  uint32_t ip_recv;
  connected_node *curr_client = connected_clients->nodes;
  connected_node *new_client;

  // Receiving the IP and the port
  read(newsock, &port_recv, sizeof(port_recv));
  port_recv = ntohs(port_recv);
  read(newsock, &ip_recv, sizeof(ip_recv));
  ip_recv = ntohl(ip_recv);

  // struct in_addr ip_addr;
  // ip_addr.s_addr = ip_recv;
  // // Inet_ntoa needs to be in network byte order, so no need to use ntohl
  // printf("IP is %s\n", inet_ntoa(ip_addr));

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

  printf("Client connected from: %d %d\n", port_recv, ip_recv);

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

  // TODO Sending USER_ON messages to every connected client
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

  // Sending the id of every connected client
  curr_client = connected_clients->nodes;
  while (curr_client != NULL)
  {
    printf("%d %d\n", curr_client->clientPort, curr_client->clientIP);
    port_net = htons(curr_client->clientPort);
    write(newsock, &port_net, sizeof(port_net));
    ip_net = htonl(curr_client->clientIP);
    write(newsock, &ip_net, sizeof(ip_net));

    curr_client = curr_client->next;
  }

  free(buffer);
  free(count_str);
}

int main(int argc, char **argv){
  int portNum, sock, newsock, optval = 1, hostname, i;
  int activity, clients[MAX_CLIENTS], max, sd;
  char hostbuffer[256];
  char *IPbuffer, *command_buffer;
  struct hostent *host_entry;
  struct sockaddr_in server, client;
  struct sockaddr *serverptr = (struct sockaddr *)&server;
  struct sockaddr *clientptr = (struct sockaddr *)&client;
  socklen_t clientlen;
  fd_set readfds;
  connected_list *connected_clients;

  // Parsing the input from the command line
  if (argc != 3)
  {
    printf("Usage: ./dropbox_server -p portNum\n");
    exit(1);
  }
  if (strcmp(argv[1], "-p") != 0)
  {
    printf("Usage: ./dropbox_server -p portNum\n");
    exit(1);
  }
  portNum = atoi(argv[2]);

  // TODO add signal handler

  // Creating the socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("Socket creation");
    exit(2);
  }

  // Reusing the connections
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setsockopt failed");
    exit(2);
  }

  // Binding the socket
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(portNum);
  if (bind(sock, serverptr, sizeof(server)) < 0)
  {
    perror("Binding socket");
    exit(2);
  }

  // Listening for connections
  if (listen(sock, 5) < 0)
  {
    perror("Listening");
    exit(2);
  }

  // Printing the server's id
  if ((hostname = gethostname(hostbuffer, sizeof(hostbuffer))) == -1)
  {
    perror("gethostname failed");
    exit(2);
  }
  if ((host_entry = gethostbyname(hostbuffer)) == NULL)
  {
    perror("gethostbyname failed");
    exit(2);
  }
  IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
  if (IPbuffer == NULL)
  {
    perror("inet_ntoa failed miserably");
    exit(2);
  }
  printf("Server is ready.\nPort: %d\nIP address: %s\nHostname: %s\n\n",
          portNum, IPbuffer, hostbuffer);

  // Setting up select
  for (i = 0; i < MAX_CLIENTS; i++)
  {
    clients[i] = 0;
  }

  command_buffer = (char*)calloc(11, sizeof(char));
  if (command_buffer == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }

  connected_clients = (connected_list*)malloc(sizeof(connected_list));
  if (connected_clients == NULL)
  {
    perror("Malloc failed");
    exit(2);
  }
  connected_clients->nodes = NULL;

  // Accepting messages from the clients
  while(1)
  {
    // Creating the socket set
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    max = sock;

    // Updating the set
    for (i = 0; i < MAX_CLIENTS; i++)
    {
      sd = clients[i];

      if (sd > 0)
      {
        FD_SET(sd, &readfds);
      }

      if (sd > max)
      {
        max = sd;
      }
    }

    // Using select for I/O
    activity = select(max + 1, &readfds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR))
    {
      perror("select error");
      exit(2);
    }

    // Incoming connection
    if (FD_ISSET(sock, &readfds))
    {
      // Accept new connection
      if ((newsock = accept(sock, clientptr, &clientlen)) < 0)
      {
        perror("Accepting new connection");
        exit(2);
      }

      printf("A client has connected to the server.\n");

      // Add the client to the list of sockets
      for (i = 0; i < MAX_CLIENTS; i++)
      {
        if (clients[i] == 0)
        {
          clients[i] = newsock;
          break;
        }
      }
    }

    // I/O
    for (i = 0; i < MAX_CLIENTS; i++)
    {
      sd = clients[i];

      if (FD_ISSET(sd, &readfds))
      {
        // Someone disconnected
        if (read(sd, command_buffer, 11) == 0)
        {
          close(sd);
          clients[i] = 0;
        }
        else
        {
          // Read the command that was sent
          // and call the right function to deal with it
          if (strcmp(command_buffer, "LOG_ON") == 0)
          {
            logon(newsock, connected_clients);
          }
          else if (strcmp(command_buffer, "GET_CLIENTS") == 0)
          {
            getclients(newsock, connected_clients);
          }
          else if (strcmp(command_buffer, "LOG_OFF") == 0)
          {
            printf("GOT IT\n");
          }
        }
      }
    }

    memset(command_buffer, 0, 11);
  }

  return 0;
}
