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
  int count = -1;
  DIR *dir;
  struct dirent *ent;
  char *next_path, *count_str;
  next_path = (char*)calloc(128, sizeof(char));
  if (next_path == NULL)
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
  sprintf(count_str, "%d", count);

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
      // At first the client sends the version as -1
      // to make the communication easier
      write(fd, next_path, 128);
      write(fd, count_str, 12);
    }
  }

  if (closedir(dir) == -1)
  {
    perror("Closing the input directory failed");
    exit(2);
  }
  free(next_path);
  free(count_str);
}

void getfile(int fd, char *input_dir){
  char *pathname, *version, *correct_path, *buffer;
  correct_path = (char*)calloc(128, sizeof(char));
  if (correct_path == NULL)
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

  // Checking if the file exists at all
  if (access(correct_path, F_OK) != -1)
  {
    if (atoi(version) == -1)
    {
      // Sending the file
      printf("WILL send whole file\n");
      // Write FILE_LIST N
      strcpy(buffer, "FILE_SIZE");
      write(fd, buffer, 15);
    }
    else
    {
      // Checking the version of the local file
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
}
