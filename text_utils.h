#ifndef TEXT_UTILS_H
#define TEXT_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/*
 * Load font from file
 * if error return NULL
 */
TTF_Font* load_font(const char *font_path, int size);

/*
 * Display text on screen.
 * Automaticly make and delete texture.
 */
void render_text(SDL_Renderer *renderer,
                TTF_Font *font,
                const char *text,
                int x, int y,
                SDL_Color color);

/*
 * Display text with bg
 */
void render_text_with_bg(SDL_Renderer *renderer,
                        TTF_Font *font,
                        const char *text,
                        int x, int y,
                        SDL_Color text_color,
                        SDL_Color bg_color);

#endif
