#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv){
  int i;

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
      i++;
    }
    else if (strcmp(argv[i], "-p") == 0)
    {
      i++;
    }
    else if (strcmp(argv[i], "-w") == 0)
    {
      i++;
    }
    else if (strcmp(argv[i], "-b") == 0)
    {
      i++;
    }
    else if (strcmp(argv[i], "-sp") == 0)
    {
      i++;
    }
    else if (strcmp(argv[i], "-sip") == 0)
    {
      i++;
    }
    else
    {
      printf("Usage: ./dropbox_client -d dirName -p portNum -w workerThreads -b bufferSize -sp serverPort -sip serverIP\n");
      exit(1);
    }
  }

  return 0;
}
