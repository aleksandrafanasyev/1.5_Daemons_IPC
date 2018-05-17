.PHONY : all clean

SHELL := /bin/sh
CC := gcc
CFLAGS := -g -Wall -Werror 
LDFLAGS := -g -Wall -Werror
LIBS :=-lrt 

all: server client
       
server: server.o
	$(CC) server.o $(LDFLAGS) $(LIBS) -o server

client: client.o
	$(CC) client.o $(LDFLAGS) $(LIBS) -o client

server.o: server.c ipc.h
	$(CC) -c server.c $(CFLAGS) -o server.o

client.o: client.c ipc.h
	$(CC) -c client.c $(CFLAGS) -o client.o

clean:
	rm -f server
	rm -f client
	rm -f *.o



