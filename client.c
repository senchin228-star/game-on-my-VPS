#include "config.h"
#include "protocol.h"
#include "sdl_utils.h"
#include "text_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

int send_move(int sock, struct sockaddr_in *server_addr, player_cord cord)
{
    player_message mes = {
        .type = SEND_CORD,
        .cord = cord
    };
    int bytes_sent = sendto(sock, &mes, sizeof(mes), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent != (int)sizeof(mes)){
        perror("Failed to send the mes\n");
        return 1;
    }
    return 0;
}
int join_request(int sock, struct sockaddr_in *server_addr,char* nick)
{
    player_message request = {
        .type = PLAYER_JOIN_REQUEST,
    };
    strncpy(request.nickname, nick, sizeof(request.nickname) - 1);
    request.nickname[sizeof(request.nickname) - 1] = '\0';
    int bytes_sent = sendto(sock, &request, sizeof(request), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent != (int)sizeof(request)){
        perror("Failed to send the request\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2){
        printf("Need nickname");
        return 1;
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket create error\n");
        return 1;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0){
        perror("Wrong IP addres\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("Init SDL error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        printf("Init TTF error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    // Load font
    TTF_Font *font_large = load_font("fonts/DejaVuSans-Bold.ttf", 48);
    TTF_Font *font_medium = load_font("fonts/DejaVuSans.ttf", 32);
    TTF_Font *font_small = load_font("fonts/DejaVuSans.ttf", 24);

    if (!font_large || !font_medium || !font_small) {
        fprintf(stderr, "Failed to load fonts\n");
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {255, 255, 0, 255};


    if (IMG_Init(IMG_INIT_PNG) == 0){
        fprintf(stderr, "Error SDL2_image Initialization: %s\n", IMG_GetError());
    }

    SDL_Window* window = SDL_CreateWindow(
        "game-on-my-VPS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("Create window error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Create rederer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Event event;
    int running = 1;
    SDL_Rect *players_rects;

    server_message resp;

    SDL_Texture *connect_but_tex = make_texture(renderer, "image/connect_but.png");

    SDL_Rect join_button = { .x = 300, .y = 260, .w = 200, .h = 80 };

    while(running){
        int menu = 1;
        int ingame = 0;
        int lobby = 0;
        while(menu){
            while (SDL_PollEvent(&event)){
                if (event.type == SDL_QUIT){
                    running = 0;
                    menu = 0;
                    break;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN){
                    if (event.button.button == SDL_BUTTON_LEFT){
                        SDL_Point mouse_pos = {.x = event.button.x, .y = event.button.y};
                        if (SDL_PointInRect(&mouse_pos, &join_button)){
                            if (join_request(sock, &server_addr, argv[1]) != 0) return 1;
                        }
                    }
                }
            }
            int bytes_received = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);
            if (bytes_received == (int)sizeof(resp) &&
                resp.id != -1 && resp.type == PLAYER_JOIN_ACCEPT){
                menu = 0;
                lobby = 1;
            }
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            if (connect_but_tex != NULL){
                SDL_RenderCopy(renderer, connect_but_tex, NULL, &join_button);
            }
            else{ // if texture not exist
                SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // Green
                SDL_RenderFillRect(renderer, &join_button);
            }
            SDL_RenderPresent(renderer);
        }
        if (!running) break;
        int id = resp.id;
        player_client_info all_players[MAX_PLAYERS];
        memcpy(all_players, resp.players, sizeof(all_players));
        player_cord my_cord = resp.players[id].cord;

        int countdown_time = -1;
        while (lobby){
            while (SDL_PollEvent(&event)){
                if (event.type == SDL_QUIT){
                    lobby = 0;
                    menu = 1;
                }
            }
            int bytes_received = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);

            if(bytes_received == (int)sizeof(resp) && resp.type == MSG_LOBBY_INFO){
                countdown_time = resp.left_time;
                memset(all_players, 0, sizeof(all_players));
                memcpy(all_players, resp.players, sizeof(resp.players));
            }

            else if(bytes_received == (int)sizeof(resp) && 
                    resp.type == MSG_GAME_START){
                lobby = 0; 
                ingame = 1;
            }
            // RENDER
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            render_text(renderer, font_large, "WAITING FOR PLAYERS", 100, 50, white);
            char time_text[256] = {0};
            if (countdown_time > 0){
                snprintf(time_text, sizeof(time_text),
                    "Game starts in: %d\n", countdown_time);
                render_text(renderer, font_medium, time_text, 150, 350, yellow);
            }
            players_rects = player_cords_to_rects(all_players, MAX_PLAYERS, 50, 50);
            render_players(players_rects, all_players, MAX_PLAYERS, renderer);
            render_player_nicknames(
                players_rects,
                all_players,
                MAX_PLAYERS,
                renderer,
                font_small,
                white
            );
            if (players_rects != NULL) {
                free(players_rects);
                players_rects = NULL;
            }

            SDL_RenderPresent(renderer);
        }

        const int FPS = 60;
        const int FRAME_DELAY = 1000 / FPS;
        Uint32 frameStart;
        int frameTime;

        while(ingame){
            frameStart = SDL_GetTicks();
            while(SDL_PollEvent(&event)){
                if (event.type  == SDL_QUIT){
                    running = 0;
                    ingame = 0;
                    break;
                }
            }
            int bytes_received = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);
            if (bytes_received == (int)sizeof(resp)){
                if (resp.type == MSG_CLIENTS_INFO) {
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (i != id) {
                            all_players[i] = resp.players[i];
                        }
                    }
                    all_players[id].cord = my_cord;
                }
                else if (resp.type == MSG_GAME_OVER){
                    printf("Game over\n");
                    ingame = 0;
                }
            }
            player_cord previous_cord = my_cord;

            const Uint8 *state = SDL_GetKeyboardState(NULL);
            if (state[SDL_SCANCODE_LEFT])  my_cord.x--;
            if (state[SDL_SCANCODE_RIGHT]) my_cord.x++;
            if (state[SDL_SCANCODE_UP])    my_cord.y++;
            if (state[SDL_SCANCODE_DOWN])  my_cord.y--;
            players_rects = player_cords_to_rects(all_players, MAX_PLAYERS, 50, 50);

            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            render_players(players_rects, all_players, MAX_PLAYERS, renderer);
            render_player_nicknames(
                players_rects,
                all_players,
                MAX_PLAYERS,
                renderer,
                font_small,
                white
            );
            SDL_RenderPresent(renderer);

            if (my_cord.x != previous_cord.x ||
                my_cord.y != previous_cord.y) {
                send_move(sock, &server_addr, my_cord);
            }

            if (players_rects != NULL ) free(players_rects);

            frameTime = SDL_GetTicks() - frameStart;
            if (FRAME_DELAY > frameTime) {
                SDL_Delay(FRAME_DELAY - frameTime);
            }
        }
    }
    if (connect_but_tex) SDL_DestroyTexture(connect_but_tex);
    if (font_large) TTF_CloseFont(font_large);
    if (font_medium) TTF_CloseFont(font_medium);
    if (font_small) TTF_CloseFont(font_small);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(sock);
    return 0;
}
