#include "sdl_utils.h"

#include <stdlib.h>

SDL_Rect *player_cords_to_rects(const player_client_info *players,
                                size_t count,
                                int width,
                                int height)
{
    if (players == NULL || count == 0) {
        return NULL;
    }

    SDL_Rect *rects = malloc(count * sizeof(*rects));
    if (rects == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!players[i].ingame) continue;
        rects[i].x = players[i].cord.x;
        rects[i].y = players[i].cord.y  * -1;
        rects[i].w = width;
        rects[i].h = height;
    }

    return rects;
}

void render_players(SDL_Rect *players_rects, player_client_info *players, size_t count, SDL_Renderer *renderer)
{
    if (players_rects == NULL ||
            renderer == NULL ||
            players  == NULL)
        return;

    for (size_t i = 0; i < count; i++){
        if (!players[i].ingame) {
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 0, players[i].color.R,
                                            players[i].color.G,
                                            players[i].color.B);

        SDL_RenderFillRect(renderer, players_rects + i); 
    }
}

void render_player_nicknames(SDL_Rect *players_rects,
                             player_client_info *players,
                             size_t count,
                             SDL_Renderer *renderer,
                             TTF_Font *font,
                             SDL_Color color)
{
    if (players_rects == NULL ||
        players == NULL ||
        renderer == NULL ||
        font == NULL) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!players[i].ingame || players[i].nickname[0] == '\0') {
            continue;
        }

        SDL_Surface *surface = TTF_RenderUTF8_Blended(
            font,
            players[i].nickname,
            color
        );

        if (surface == NULL) {
            continue;
        }

        SDL_Texture *texture =
            SDL_CreateTextureFromSurface(renderer, surface);

        if (texture == NULL) {
            SDL_FreeSurface(surface);
            continue;
        }

        int text_width = 0;
        int text_height = 0;
        SDL_QueryTexture(texture, NULL, NULL, &text_width, &text_height);

        SDL_Rect text_rect = {
            .x = players_rects[i].x + (players_rects[i].w - text_width) / 2,
            .y = players_rects[i].y - text_height - 5,
            .w = text_width,
            .h = text_height
        };

        SDL_RenderCopy(renderer, texture, NULL, &text_rect);

        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
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

