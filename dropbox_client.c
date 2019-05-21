#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "types.h"
#include "client_functions.h"

pthread_mutex_t mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
pool_t pool;
volatile sig_atomic_t flag = 1;

void catchinterrupt(int signo){
  flag = 0;
  printf("Caught %d. Sending LOG_OFF message, cleaning memory and exiting.\n", signo);
}

void initialize(pool_t *pool, int bufferSize){
  pool->buffer = (circular_node*)malloc(bufferSize * sizeof(circular_node));
  if (pool->buffer == NULL)
  {
    perror("Malloc failed");
    exit(2);
  }
  pool->start = 0;
  pool->end = -1;
  pool->count = 0;
  pool->size = bufferSize;
}

void place(pool_t *pool, circular_node data){
  // Waiting until there is room in the buffer
  pthread_mutex_lock(&mtx);
  while (pool->count >= pool->size)
  {
    pthread_cond_wait(&cond_nonfull, &mtx);
  }

  // Placing the item in the buffer
  pool->end = (pool->end + 1) % pool->size;
  pool->buffer[pool->end] = data;
  pool->count++;
  pthread_mutex_unlock(&mtx);
}

circular_node obtain(pool_t *pool){
  circular_node node;

  // Waiting until there is something in the buffer
  pthread_mutex_lock(&mtx);
  while (pool->count <= 0)
  {
    pthread_cond_wait(&cond_nonempty, &mtx);
  }

  // Removing an item
  node = pool->buffer[pool->start];
  pool->start = (pool->start + 1) % pool->size;
  pool->count--;
  pthread_mutex_unlock(&mtx);

  return node;
}

void * worker(void *ptr){
  int sock, i, x;
  char *input, *num, *reply, *pathname;
  circular_node node, data;
  struct hostent *rem;
  struct in_addr ip_addr;
  struct sockaddr_in server;
  struct sockaddr *serverptr = (struct sockaddr*)&server;

  input = (char*)calloc(13, sizeof(char));
  if (input == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  num = (char*)calloc(12, sizeof(char));
  if (num == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  reply = (char*)calloc(15, sizeof(char));
  if (reply == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  pathname = (char*)calloc(128, sizeof(char));
  if (pathname == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }

  while (flag == 1)
  {
    node = obtain(&pool);
    pthread_cond_signal(&cond_nonfull);

    // GET_FILE_LIST request
    if (strcmp(node.pathname, "-1") == 0)
    {
      // Creating the socket to communicate with the server
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
        perror("Creating socket");
        exit(2);
      }

      ip_addr.s_addr = htonl(node.ip);

      // Connecting to the server
      if ((rem = gethostbyname(inet_ntoa(ip_addr))) == NULL)
      {
        perror("gethostbyname failed");
        exit(2);
      }
      server.sin_family = AF_INET;
      memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
      server.sin_port = htons(node.port);
      if (connect(sock, serverptr, sizeof(server)) < 0)
      {
        perror("Connecting to server");
        exit(2);
      }

      // Sending GET_FILE_LIST request
      strcpy(input, "GET_FILE_LIST");
      write(sock, input, 13);

      // Reading the requested data
      read(sock, reply, 15);
      if (strcmp(reply, "FILE_LIST") == 0)
      {
        memset(reply, 0, 12);
        read(sock, num, 12);
        x = atoi(num);
        for (i = 0; i < x; i++)
        {
          memset(pathname, 0, 128);
          memset(num, 0, 12);
          read(sock, pathname, 128);
          read(sock, num, 12);

          strcpy(data.pathname, pathname);
          strcpy(data.version, num);
          data.port = node.port;
          data.ip = node.ip;
          place(&pool, data);
          pthread_cond_signal(&cond_nonempty);
        }
      }
      else
      {
        printf("GET_FILE_LIST error\n");
        pthread_exit(0);
      }

      close(sock);
    }
    else
    {
      // GET_FILE request
      printf("FILE\n");
    }

    memset(input, 0, 13);
  }

  free(input);
  free(reply);
  free(num);
  free(pathname);
  pthread_exit(0);
}

int main(int argc, char **argv){
  int i, server_port, server_sock, portNum, serverNum, clients[MAX_CLIENTS];
  int workerThreads, bufferSize, comm, optval = 1, x, max, sd, activity, newsock;
  uint16_t port_net;
  uint32_t ip_net;
  struct sockaddr_in server, temp, comm_addr, client;
  struct sockaddr *serverptr = (struct sockaddr*)&server;
  struct sockaddr *commptr = (struct sockaddr*)&comm_addr;
  struct sockaddr *clientptr = (struct sockaddr *)&client;
  struct hostent *rem, *host_entry;
  static struct sigaction act;
  socklen_t clientlen;
  char *command_buffer, *IPbuffer, *number_recv, *input;
  char dirName[256], serverIP[256], hostbuffer[256];
  connected_list *client_list;
  connected_node *curr_client, *new_client;
  pthread_t thr;
  fd_set readfds;

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

  // Setting up the signal handler
  act.sa_handler = catchinterrupt;
  sigfillset(&(act.sa_mask));
  sigaction(SIGINT, &act, NULL);

  // Creating the socket to communicate with other clients
  if ((comm = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("Socket creation");
    exit(2);
  }

  // Reusing the connections
  if (setsockopt(comm, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setsockopt failed");
    exit(2);
  }

  // Binding the socket
  comm_addr.sin_family = AF_INET;
  comm_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  comm_addr.sin_port = htons(portNum);
  if (bind(comm, commptr, sizeof(comm_addr)) < 0)
  {
    perror("Binding socket");
    exit(2);
  }

  // Listening for connections
  if (listen(comm, 5) < 0)
  {
    perror("Listening");
    exit(2);
  }

  // Creating the socket to communicate with the server
  if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("Creating socket");
    exit(2);
  }

  // Connecting to the server
  if ((rem = gethostbyname(serverIP)) == NULL)
  {
    perror("gethostbyname failed");
    exit(2);
  }
  server.sin_family = AF_INET;
  memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
  server.sin_port = htons(server_port);
  if (connect(server_sock, serverptr, sizeof(server)) < 0)
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
  number_recv = (char*)calloc(12, sizeof(char));
  if (number_recv == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  input = (char*)calloc(13, sizeof(char));
  if (input == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  client_list = (connected_list*)malloc(sizeof(connected_list));
  if (client_list == NULL)
  {
    perror("Malloc failed");
    exit(2);
  }
  client_list->nodes = NULL;
  // TODO make list thread safe

  printf("The client is ready.\n");

  // LOG_ON message
  strcpy(command_buffer, "LOG_ON");
  if (write(server_sock, command_buffer, 11) < 0)
  {
    perror("writing LOG_ON failed");
    exit(2);
  }
  memset(command_buffer, 0, 11);

  port_net = htons(portNum);
  write(server_sock, &port_net, sizeof(port_net));

  // Find the IP address of the client and send it
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
  ip_net = temp.sin_addr.s_addr;
  serverNum = temp.sin_addr.s_addr;
  write(server_sock, &ip_net, sizeof(ip_net));

  // GET_CLIENTS message
  strcpy(command_buffer, "GET_CLIENTS");
  write(server_sock, command_buffer, 11);
  memset(command_buffer, 0, 11);

  read(server_sock, command_buffer, 11);
  if (strcmp(command_buffer, "CLIENT_LIST") == 0)
  {
    read(server_sock, number_recv, 12);
    x = atoi(number_recv);
    memset(number_recv, 0, 12);

    for (i = 0; i < x; i++)
    {
      // Receiving the IP and the port
      read(server_sock, &port_net, sizeof(port_net));
      port_net = ntohs(port_net);
      read(server_sock, &ip_net, sizeof(ip_net));
      ip_net = ntohl(ip_net);

      if ((port_net == portNum) && (ip_net == ntohl(serverNum)))
      {
        continue;
      }

      //Creating a new client node
      new_client = (connected_node*)malloc(sizeof(connected_node));
      if (new_client == NULL)
      {
        perror("Malloc failed");
        exit(2);
      }
      new_client->clientIP = ip_net;
      new_client->clientPort = port_net;
      new_client->next = NULL;

      // Updating the list
      if (client_list->nodes == NULL)
      {
        client_list->nodes = new_client;
      }
      else
      {
        curr_client = client_list->nodes;
        while ((curr_client->next) != NULL)
        {
          curr_client = curr_client->next;
        }

        curr_client->next = new_client;
      }
    }
  }
  else
  {
    printf("GET_CLIENTS failed\n");
    exit(3);
  }

  // Initializing the buffer and the threads
  initialize(&pool, bufferSize);
  pthread_mutex_init(&mtx, 0);
  pthread_cond_init(&cond_nonempty, 0);
  pthread_cond_init(&cond_nonfull, 0);
  for (i = 0; i < workerThreads; i++)
  {
    pthread_create(&thr, 0, worker, 0);
  }

  // Adding the clients to the buffer
  curr_client = client_list->nodes;
  while (curr_client != NULL)
  {
    circular_node data;
    strcpy(data.pathname, "-1");
    strcpy(data.version, "-1");
    data.port = curr_client->clientPort;
    data.ip = curr_client->clientIP;
    place(&pool, data);

    curr_client = curr_client->next;
  }

  // Setting up select
  for (i = 0; i < MAX_CLIENTS; i++)
  {
    clients[i] = 0;
  }

  // Accepting messages from the clients
  while(flag == 1)
  {
    // Creating the socket set
    FD_ZERO(&readfds);
    FD_SET(comm, &readfds);
    FD_SET(server_sock, &readfds);
    max = server_sock;

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
    if (flag == 0)
    {
      break;
    }
    if ((activity < 0) && (errno != EINTR))
    {
      perror("select error");
      exit(2);
    }

    // Incoming connection
    if (FD_ISSET(comm, &readfds))
    {
      // Accept new connection
      newsock = accept(comm, clientptr, &clientlen);
      if (flag == 0)
      {
        break;
      }
      if (newsock < 0)
      {
        perror("Accepting new connection");
        exit(2);
      }

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

    // I/O happened
    for (i = 0; i < MAX_CLIENTS; i++)
    {
      sd = clients[i];

      if (FD_ISSET(sd, &readfds))
      {
        // Someone disconnected
        if (read(sd, input, 13) == 0)
        {
          close(sd);
          clients[i] = 0;
        }
        else
        {
          // Responding to client requests
          if (strcmp(input, "GET_FILE_LIST") == 0)
          {
            getfilelist(sd, dirName);
          }
          else if (strcmp(input, "GET_FILE") == 0)
          {
          }
          else if (strcmp(input, "USER_OFF") == 0)
          {
          }
          else if (strcmp(input, "USER_ON") == 0)
          {
          }
        }
      }
    }

    memset(input, 0, 13);
  }

  printf("Loggin off.\n");

  for (i = 0; i < workerThreads; i++)
  {
    // Maybe array?
    // pthread_join(thr, 0);
  }
  pthread_cond_destroy(&cond_nonempty);
  pthread_cond_destroy(&cond_nonfull);
  pthread_mutex_destroy(&mtx);

  // LOG_OFF message
  memset(command_buffer, 0, 11);
  strcpy(command_buffer, "LOG_OFF");
  write(server_sock, command_buffer, 11);
  port_net = htons(portNum);
  write(server_sock, &port_net, sizeof(port_net));
  write(server_sock, &serverNum, sizeof(serverNum));

  close(server_sock);
  close(comm);
  free(command_buffer);
  free(number_recv);

  return 0;
}
