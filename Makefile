CC=gcc
CFLAGS= -c -Wall

all:    dropbox_client

dropbox_client:   dropbox_client.o
	$(CC)  dropbox_client.o -o dropbox_client

dropbox_client.o:   dropbox_client.c
	$(CC)  $(CFLAGS) dropbox_client.c

clean:
	rm -f   \
		dropbox_client.o dropbox_client

