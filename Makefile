CC=gcc
CFLAGS= -c -Wall

all:    dropbox_client \
	dropbox_server

dropbox_client:   dropbox_client.o
	$(CC)  dropbox_client.o -o dropbox_client

dropbox_server:   dropbox_server.o
	$(CC)  dropbox_server.o -o dropbox_server

dropbox_client.o:   dropbox_client.c
	$(CC)  $(CFLAGS) dropbox_client.c

dropbox_server.o:   dropbox_server.c
	$(CC)  $(CFLAGS) dropbox_server.c

clean:
	rm -f   \
		dropbox_client.o dropbox_client \
		dropbox_server.o dropbox_server

