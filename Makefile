CC = gcc
CFLAGS = -Wall -Wextra -g

.PHONY: all clean

all: server client

server: server.c config.h
	$(CC) $(CFLAGS) server.c -o server

client: client.c client.h
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f server client
