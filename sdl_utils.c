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
        rects[i].y = cords[i].y;
        rects[i].w = width;
        rects[i].h = height;
    }

    return rects;
}
