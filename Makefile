CC = gcc
CFLAGS = -Wall -Wextra -g
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)

.PHONY: all clean

all: server client

server: server.c config.h protocol.h display.h display.c
	$(CC) $(CFLAGS) server.c display.c -o server

client: client.c config.h protocol.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) client.c -o client $(SDL_LIBS)

clean:
	rm -f server client
