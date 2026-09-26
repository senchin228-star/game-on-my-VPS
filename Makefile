CC = gcc
CFLAGS = -Wall -Wextra -g
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf

.PHONY: all clean

all: server client

server: server.c config.h protocol.h server_utils.h server_utils.c
	$(CC) $(CFLAGS) server.c server_utils.c -o server

client: client.c sdl_utils.c config.h protocol.h sdl_utils.h text_utils.h text_utils.c image fonts
	$(CC) $(CFLAGS) client.c sdl_utils.c text_utils.c -o client $(SDL_LIBS) $(SDL_CFLAGS)

clean:
	rm -f server client
