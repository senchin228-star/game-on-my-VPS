#ifndef SDL_UTILS_H
#define SDL_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>

#include "protocol.h"

/*
 * Convert player coordinates to SDL rectangles.
 *
 * The returned array contains `count` rectangles and must be freed with
 * free() by the caller. NULL is returned when cords is NULL, count is zero,
 * or the allocation fails.
 */
SDL_Rect *player_cords_to_rects(const player_client_info *players,
                                size_t count,
                                int width,
                                int height);

/*
 * Render player rectangles onto the screen.
 *
 * This function draws filled rectangles for each player using the
 * provided SDL_Renderer. It expects valid pointers to both the
 * rectangles array and the renderer.
 */
void render_players(SDL_Rect *players_rects, player_client_info *players, size_t count, SDL_Renderer *renderer);

/*
 * Render player nicknames above their squares.
 *
 * This function displays the nickname of each player above their square.
 * It expects valid pointers to rectangles, players array, font, and renderer.
 */
void render_player_nicknames(SDL_Rect *players_rects, player_client_info *players, size_t count, SDL_Renderer *renderer, TTF_Font *font, SDL_Color color);

/**
 * * Create an SDL_Texture from an image file.
 * *
 * * This function loads an image from the specified directory path into
 * * memory, converts it into a hardware-accelerated SDL_Texture using
 * * the provided renderer, and safely frees temporary surface resources.
 * * It expects a valid renderer pointer and a non-null file directory string.
 * */
SDL_Texture* make_texture(SDL_Renderer *renderer, char *dir);

#endif
