#ifndef SDL_UTILS_H
#define SDL_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>

#include "protocol.h"

/* Convert one game-space apple coordinate to a heap-allocated SDL rectangle.
 * The caller owns the result and must free it; NULL indicates failure.
 */
SDL_Rect *apple_cords_to_rects(const player_cord *apple_cords,
                                int width,
                                int height);

/* Draw one filled apple rectangle. */
void render_apple(SDL_Rect *apple_rect, SDL_Renderer *renderer);

/* Convert player coordinates to an indexed SDL rectangle array.
 * Inactive-player entries are uninitialized; the caller must skip them.
 * The caller owns the returned array and must free it.
 */
SDL_Rect *player_cords_to_rects(const player_client_info *players,
                                size_t count,
                                int width,
                                int height);

/* Draw active players using the rectangles at matching player indices. */
void render_players(SDL_Rect *players_rects, player_client_info *players, size_t count, SDL_Renderer *renderer);

/* Draw nonempty player nicknames above their matching rectangles. */
void render_player_nicknames(SDL_Rect *players_rects, player_client_info *players, size_t count, SDL_Renderer *renderer, TTF_Font *font, SDL_Color color);

/* Load an image as a texture; return NULL on failure and free the temporary surface. */
SDL_Texture* make_texture(SDL_Renderer *renderer, char *dir);

#endif
