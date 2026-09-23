#include "sdl_utils.h"

#include <stdlib.h>

SDL_Rect *player_cords_to_rects(const player_cord *cords,
                                size_t count,
                                int width,
                                int height)
{
    if (cords == NULL || count == 0) {
        return NULL;
    }

    SDL_Rect *rects = malloc(count * sizeof(*rects));
    if (rects == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        rects[i].x = cords[i].x;
        rects[i].y = cords[i].y  * -1;
        rects[i].w = width;
        rects[i].h = height;
    }

    return rects;
}

void render_players(SDL_Rect *players_rects, size_t count, SDL_Renderer *renderer)
{
    if (players_rects == NULL || renderer == NULL) printf("Wrong arg in render_players");
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // Green

    for (size_t i = 0; i < count; i++){
        SDL_RenderFillRect(renderer, players_rects + i); 
    }
}

SDL_Texture* make_texture(SDL_Renderer *renderer, char *dir)
{
    if (dir == NULL) return NULL;
    SDL_Surface *surface = IMG_Load(dir);
    if (surface == NULL){
        fprintf(stderr, "Error create surface for %s: %s\n",dir, IMG_GetError());
        return NULL;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL){
        fprintf(stderr, "Error create texture for %s: %s\n", dir, SDL_GetError());
        return NULL;
    }
    SDL_FreeSurface(surface);
    return texture;
}

