CC=gcc
CFLAGS= -c -Wall

all:    dropbox_client \
	dropbox_server

dropbox_client:   dropbox_client.o client_functions.o
	$(CC)  dropbox_client.o client_functions.o -o dropbox_client -lpthread

dropbox_server:   dropbox_server.o server_functions.o
	$(CC)  dropbox_server.o server_functions.o -o dropbox_server

server_functions.o:   server_functions.c server_functions.h
	$(CC)  $(CFLAGS) server_functions.c

client_functions.o:   client_functions.c client_functions.h
	$(CC)  $(CFLAGS) client_functions.c

dropbox_client.o:   dropbox_client.c
	$(CC)  $(CFLAGS) dropbox_client.c -lpthread

dropbox_server.o:   dropbox_server.c
	$(CC)  $(CFLAGS) dropbox_server.c

clean:
	rm -f   \
		dropbox_client.o dropbox_client \
		dropbox_server.o dropbox_server \
		server_functions.o \
		client_functions.o

