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
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include "types.h"
#include "client_functions.h"

void getfilelist(int fd, char *input_dir){
  char *buffer, *count_str;
  int count;
  buffer = (char*)calloc(15, sizeof(char));
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

  // Write FILE_LIST N
  strcpy(buffer, "FILE_LIST");
  write(fd, buffer, 15);

  count = count_files(input_dir);

  sprintf(count_str, "%d", count);
  write(fd, count_str, 12);

  // Recursively send every pathname
  pathnames(input_dir, fd);

  free(count_str);
  free(buffer);
}

int count_files(char *input_dir){
  DIR *dir;
  struct dirent *ent;
  char next_path[128];
  int count = 0;

  dir = opendir(input_dir);
  if (dir == NULL)
  {
    perror("Count files");
    exit(2);
  }

  while ((ent = readdir(dir)) != NULL)
  {
    sprintf(next_path, "%s/%s", input_dir, ent->d_name);
    if (ent->d_type == DT_DIR)
    {
      if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0))
      {
        continue;
      }

      // We need to go deeper
      count += count_files(next_path);
    }
    else
    {
      count++;
    }
  }

  if (closedir(dir) == -1)
  {
    perror("Closing the input directory failed");
    exit(2);
  }

  return count;
}

void pathnames(char *input_dir, int fd){
  DIR *dir;
  struct dirent *ent;
  char *next_path, *version_str;
  next_path = (char*)calloc(128, sizeof(char));
  if (next_path == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  version_str = (char*)calloc(12, sizeof(char));
  if (version_str == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }

  dir = opendir(input_dir);
  if (dir == NULL)
  {
    perror("Count files");
    exit(2);
  }

  while ((ent = readdir(dir)) != NULL)
  {
    memset(next_path, 0, 128);
    sprintf(next_path, "%s/%s", input_dir, ent->d_name);
    if (ent->d_type == DT_DIR)
    {
      if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0))
      {
        continue;
      }

      // We need to go deeper
      pathnames(next_path, fd);
    }
    else
    {
      // Sending pathname and file version
      memset(version_str, 0, 12);
      sprintf(version_str, "%u", fileversion(next_path));
      write(fd, next_path, 128);
      write(fd, version_str, 12);
    }
  }

  if (closedir(dir) == -1)
  {
    perror("Closing the input directory failed");
    exit(2);
  }
  free(next_path);
  free(version_str);
}

void getfile(int fd, char *input_dir){
  char *pathname, *version, *correct_path, *buffer, *num, *send_b;
  char checkvrs[12] = {0};
  int fileLength, n;
  FILE* fp = NULL;
  send_b = (char*)calloc(1, sizeof(char));
  if (send_b == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  correct_path = (char*)calloc(128, sizeof(char));
  if (correct_path == NULL)
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
  buffer = (char*)calloc(15, sizeof(char));
  if (buffer == NULL)
  {
    perror("Calloc failed");
    exit(2);
  }
  version = (char*)calloc(12, sizeof(char));
  if (version == NULL)
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

  // Reading the rest of the request
  read(fd, pathname, 128);
  read(fd, version, 12);

  // Creating the correct path: inputDir/pathname
  sprintf(correct_path, "%s/%s", input_dir, pathname);

  // Checking version of requested file
  sprintf(checkvrs, "%u", fileversion(correct_path));

  // Checking if the file exists at all
  if (access(correct_path, F_OK) != -1)
  {
    // Check if the client has the correct version of the file
    if ((strcmp(version, checkvrs) != 0) || (strcmp(version, "-1") == 0))
    {
      // Write FILE_SIZE version
      strcpy(buffer, "FILE_SIZE");
      write(fd, buffer, 15);
      write(fd, checkvrs, 12);

      // Calculating and sending the size of the file
      fp = fopen(correct_path, "r");
      if (fp == NULL)
      {
        perror("Could not open file");
        exit(2);
      }
      fseek(fp, 0L, SEEK_END);
      fileLength = (int)ftell(fp);
      sprintf(num, "%d", fileLength);

      // Sending the size of the file
      write(fd, num, 12);

      // Using fgets to read the file byte by byte and send it
      fseek(fp, 0L, SEEK_SET);
      while ((n = fread(send_b, sizeof(char), 1, fp)) > 0)
      {
        write(fd, send_b, n);
      }

      fclose(fp);
    }
    else
    {
      strcpy(buffer, "FILE_UP_TO_DATE");
      write(fd, buffer, 15);
    }
  }
  else
  {
    strcpy(buffer, "FILE_NOT_FOUND");
    write(fd, buffer, 15);
  }

  free(pathname);
  free(version);
  free(correct_path);
  free(buffer);
  free(num);
  free(send_b);
}

void createfile(char *pathname){
  char path[128] = {0};
  char *token, *rest = NULL;
  int i, s = 0;

  // Counts how many '/' exist in the path, to count how many directories
  // will have to be created
  for (i = 0; i < (int)strlen(pathname); i++)
  {
    if (pathname[i] == '/')
    {
      s++;
    }
  }

  if (s == 0)
  {
    return;
  }

  // Using strtok_r we break down the pathname and create all the directories
  token = strtok_r(pathname, "/", &rest);
  strcpy(path, token);
  if (mkdir(path, 0777) == -1)
  {
    if (errno != EEXIST)
    {
      perror("mirror mkdir failed");
      exit(2);
    }
  }
  s--;

  while ((token != NULL) && (s > 0))
  {
    token = strtok_r(NULL, "/", &rest);
    sprintf(path, "%s/%s", path, token);
    if (mkdir(path, 0777) == -1)
    {
      if (errno != EEXIST)
      {
        perror("mirror mkdir failed");
        exit(2);
      }
    }
    s--;
  }
}

void useroff(int sock, connected_list *connected_clients, pthread_mutex_t *mtx){
  connected_node ** nodes_ptr = &(connected_clients->nodes);
  connected_node *tmp;
  uint16_t port_net;
  uint32_t ip_net;

  // Receiving the port and the IP
  read(sock, &port_net, sizeof(port_net));
  port_net = ntohs(port_net);
  read(sock, &ip_net, sizeof(ip_net));
  ip_net = ntohl(ip_net);

  // Remove client from the list
  pthread_mutex_lock(mtx);
  while (*nodes_ptr != NULL)
  {
    if (((**nodes_ptr).clientIP == ip_net) && ((**nodes_ptr).clientPort == port_net))
    {
      tmp = *nodes_ptr;
      *nodes_ptr = tmp->next;
      free(tmp);
    }
    else
    {
      nodes_ptr = &((**nodes_ptr).next);
    }
  }
  pthread_mutex_unlock(mtx);

  printf("Disconnecting from client: %d %d.\n", port_net, ip_net);
}

// Using the Jenkins One At A Time Hash for the file version
// Source: https://en.wikipedia.org/wiki/Jenkins_hash_function
uint32_t fileversion(char *path){
  uint32_t hash = 0, c, fileLength;
  FILE *fp = NULL;

  fp = fopen(path, "rb+");
  if (fp == NULL)
  {
    perror("Could not open file");
    exit(2);
  }
  fseek(fp, 0L, SEEK_END);
  fileLength = (int)ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  // Using fgetc to read the file byte by byte and calculate its hash/version
  while (fileLength > 0)
  {
    c = fgetc(fp);
    hash += c;
    hash += (hash << 10);
    hash ^= (hash >> 6);
    fileLength--;
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}
