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

int main(int argc, char **argv){
  int portNum, sock, newsock, optval = 1, hostname, i;
  int activity, clients[45], max, sd;
  uint16_t port_recv;
  char hostbuffer[256];
  char *IPbuffer, *command_buffer;
  struct hostent *host_entry;
  struct sockaddr_in server, client;
  socklen_t clientlen;
  struct sockaddr *serverptr = (struct sockaddr *)&server;
  struct sockaddr *clientptr = (struct sockaddr *)&client;
  fd_set readfds;

  // Parsing the input from the command line
  if (argc != 3)
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
  for (i = 0; i < 45; i++)
  {
    clients[i] = 0;
  }

  command_buffer = (char*)calloc(11, sizeof(char));
  if (command_buffer == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }

  // Accepting messages from the clients
  while(1)
  {
    // Creating the socket set
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    max = sock;

    // Updating the set
    for (i = 0; i < 45; i++)
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
      for (i = 0; i < 45; i++)
      {
        if (clients[i] == 0)
        {
          clients[i] = newsock;
          break;
        }
      }
    }

    // I/O
    for (i = 0; i < 45; i++)
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
          // Read the message that was sent
          if (strcmp(command_buffer, "LOG_ON") == 0)
          {
            printf("YEET\n");
          }
          read(newsock, &port_recv, sizeof(port_recv));
          port_recv = ntohs(port_recv);
          printf("Read: %s %d\n", command_buffer, port_recv);
        }
      }
    }

    memset(command_buffer, 0, 11);
  }

  return 0;
}
