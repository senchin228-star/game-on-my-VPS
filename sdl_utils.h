#ifndef SDL_UTILS_H
#define SDL_UTILS_H

#include <SDL2/SDL.h>
#include <stddef.h>

#include "protocol.h"

/*
 * Convert player coordinates to SDL rectangles.
 *
 * The returned array contains `count` rectangles and must be freed with
 * free() by the caller. NULL is returned when cords is NULL, count is zero,
 * or the allocation fails.
 */
SDL_Rect *player_cords_to_rects(const player_cord *cords,
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
void render_players(SDL_Rect *players_rects, size_t count, SDL_Renderer *renderer);
#endif
