CC = gcc
CFLAGS = -Wall -Wextra -g
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs) -lSDL2_image

.PHONY: all clean

all: server client

server: server.c config.h protocol.h display.h display.c
	$(CC) $(CFLAGS) server.c display.c -o server

client: client.c sdl_utils.c config.h protocol.h sdl_utils.h image
	$(CC) $(CFLAGS) client.c sdl_utils.c -o client $(SDL_LIBS) $(SDL_CFLAGS)

clean:
	rm -f server client
