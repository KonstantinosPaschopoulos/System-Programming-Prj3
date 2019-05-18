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
#include "types.h"

pthread_mutex_t mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
pool_t pool;
volatile sig_atomic_t flag = 1;

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
  printf("Thread has been created\n");
  circular_node node;

  while (flag == 1)
  {
    node = obtain(&pool);

    printf("%d\n", node.port);
  }

  pthread_exit(0);
}

int main(int argc, char **argv){
  int i, server_port, server_sock, portNum, serverNum;
  int workerThreads, bufferSize, comm, optval = 1, x;
  uint16_t port_net;
  uint32_t ip_net;
  struct sockaddr_in server, temp, comm_addr;
  struct sockaddr *serverptr = (struct sockaddr*)&server;
  struct sockaddr *commptr = (struct sockaddr*)&comm_addr;
  struct hostent *rem, *host_entry;
  char *command_buffer, *IPbuffer, *number_recv;
  char dirName[256], serverIP[256], hostbuffer[256];
  connected_list *client_list;
  connected_node *curr_client, *new_client;
  pthread_t thr;

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

  // Finding the server's address
  if ((rem = gethostbyname(serverIP)) == NULL)
  {
    perror("gethostbyname failed");
    exit(2);
  }

  // Connecting to the server
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

    printf("Connected clients: %d\n", x);
    for (i = 0; i < x; i++)
    {
      // Receiving the IP and the port
      read(server_sock, &port_net, sizeof(port_net));
      port_net = ntohs(port_net);
      read(server_sock, &ip_net, sizeof(ip_net));
      ip_net = ntohl(ip_net);

      printf("%d %d\n", port_net, ip_net);

      if ((port_net == portNum) && (ip_net == serverNum))
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

  // The client is ready to sync with the other clients
  

  getchar();
  printf("Loggin off\n");

  flag = 0;

  for (i = 0; i < workerThreads; i++)
  {
    // Maybe array?
    // pthread_join(thr, 0);
  }
  pthread_cond_destroy(&cond_nonempty);
  pthread_cond_destroy(&cond_nonfull);
  pthread_mutex_destroy(&mtx);

  // LOG_OFF message
  strcpy(command_buffer, "LOG_OFF");
  write(server_sock, command_buffer, 11);

  close(server_sock);
  close(comm);
  free(command_buffer);
  free(number_recv);

  return 0;
}
