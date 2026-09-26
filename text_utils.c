#include "text_utils.h"
#include <stdio.h>
#include <stdlib.h>

TTF_Font *load_font(const char *font_path, int size)
{
    if (font_path == NULL) {
        fprintf(stderr, "Font path is NULL\n");
        return NULL;
    }

    TTF_Font *font = TTF_OpenFont(font_path, size);

    if (font == NULL) {
        fprintf(stderr,
                "Failed to load font '%s': %s\n",
                font_path,
                TTF_GetError());
    }

    return font;
}

void render_text(SDL_Renderer *renderer,
                 TTF_Font *font,
                 const char *text,
                 int x,
                 int y,
                 SDL_Color color)
{
    if (renderer == NULL || font == NULL || text == NULL) {
        return;
    }

    SDL_Surface *surface =
        TTF_RenderText_Blended(font, text, color);

    if (surface == NULL) {
        fprintf(stderr,
                "Error rendering text: %s\n",
                TTF_GetError());
        return;
    }

    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    if (texture == NULL) {
        fprintf(stderr,
                "Error creating texture: %s\n",
                SDL_GetError());

        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect destination = {
        .x = x,
        .y = y,
        .w = surface->w,
        .h = surface->h
    };

    SDL_FreeSurface(surface);

    SDL_RenderCopy(
        renderer,
        texture,
        NULL,
        &destination
    );

    SDL_DestroyTexture(texture);
}

void render_text_with_bg(SDL_Renderer *renderer,
                        TTF_Font *font,
                        const char *text,
                        int x, int y,
                        SDL_Color text_color,
                        SDL_Color bg_color)
{
    if (renderer == NULL || font == NULL || text == NULL) {
        return;
    }

    SDL_Surface *surface = TTF_RenderText_Shaded(font, text, text_color, bg_color);
    if (surface == NULL) {
        fprintf(stderr, "Error rendering text: %s\n", TTF_GetError());
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (texture == NULL) {
        fprintf(stderr, "Error creating texture: %s\n", SDL_GetError());
        return;
    }

    SDL_Rect dst = {.x = x, .y = y};
    SDL_QueryTexture(texture, NULL, NULL, &dst.w, &dst.h);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}
