#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

int main(int argc, char **argv){
  pid_t new_connection;
  int portNum, sock, newsock, len, optval = 1, hostname;
  char buffer[1001], hostbuffer[400];
  char *IPbuffer;
  struct hostent *host_entry;
  struct sockaddr_in server, client;
  socklen_t clientlen;
  struct sockaddr *serverptr = (struct sockaddr *)&server;
  struct sockaddr *clientptr = (struct sockaddr *)&client;

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

  // Reusing the ports for developments sake
  setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

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
  if (listen(sock, 10) < 0)
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

  // Accepting messages from the clients
  while(1)
  {
    // Accept new connection
    if ((newsock = accept(sock, clientptr, &clientlen)) < 0)
    {
      perror("Accepting new connection");
      exit(2);
    }

    new_connection = fork();
    if (new_connection < 0)
    {
      perror("New connection fork");
      exit(2);
    }
    if (new_connection == 0)
    {
      close(sock);
      // TODO Write child function
      len = read(newsock, buffer, sizeof(buffer) - 1);
      buffer[len] = '\0';

      printf("Read: %s\n", buffer);
      close(newsock);
      exit(0);
    }
    close(newsock);
  }

  return 0;
}
