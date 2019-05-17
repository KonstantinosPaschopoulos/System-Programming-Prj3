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
  int i, server_port, sock, portNum, workerThreads, bufferSize;
  uint16_t port_send;
  uint32_t ip_send;
  struct sockaddr_in server, temp;
  struct sockaddr *serverptr = (struct sockaddr*)&server;
  struct hostent *rem, *host_entry;
  char *command_buffer, *IPbuffer;
  char dirName[256], serverIP[256], hostbuffer[256];

  // Parsing the input from the command line
  if (argc != 13)
  {
    printf("Usage: ./dropbox_client -d dirName -p portNum -w workerThreads -b bufferSize -sp serverPort -sip serverIP\n");
    exit(1);
  }
  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-d") == 0)
    {
      strcpy(dirName, argv[i + 1]);
      i++;
    }
    else if (strcmp(argv[i], "-p") == 0)
    {
      portNum = atoi(argv[i + 1]);
      i++;
    }
    else if (strcmp(argv[i], "-w") == 0)
    {
      workerThreads = atoi(argv[i + 1]);
      i++;
    }
    else if (strcmp(argv[i], "-b") == 0)
    {
      bufferSize = atoi(argv[i + 1]);
      i++;
    }
    else if (strcmp(argv[i], "-sp") == 0)
    {
      server_port = atoi(argv[i + 1]);
      i++;
    }
    else if (strcmp(argv[i], "-sip") == 0)
    {
      strcpy(serverIP, argv[i + 1]);
      i++;
    }
    else
    {
      printf("Usage: ./dropbox_client -d dirName -p portNum -w workerThreads -b bufferSize -sp serverPort -sip serverIP\n");
      exit(1);
    }
  }

  // Creating the socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("Creating socket");
    exit(2);
  }

  // Finding the server address
  if ((rem = gethostbyname(serverIP)) == NULL)
  {
    perror("gethostbyname failed");
    exit(2);
  }

  // Connecting to the server
  server.sin_family = AF_INET;
  memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
  server.sin_port = htons(server_port);
  if (connect(sock, serverptr, sizeof(server)) < 0)
  {
    perror("Connecting to server");
    exit(2);
  }

  command_buffer = (char*)calloc(11, sizeof(char));
  if (command_buffer == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }

  // LOG ON message
  strcpy(command_buffer, "LOG_ON");
  if (write(sock, command_buffer, 11) < 0)
  {
    perror("writing LOG_ON failed");
    exit(2);
  }

  port_send = htons(portNum);
  write(sock, &port_send, sizeof(port_send));

  // Find the IP address of the client
  if (gethostname(hostbuffer, sizeof(hostbuffer)) == -1)
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
  inet_pton(AF_INET, IPbuffer, &(temp.sin_addr));
  ip_send = temp.sin_addr.s_addr;
  write(sock, &ip_send, sizeof(ip_send));

  close(sock);

  return 0;
}
