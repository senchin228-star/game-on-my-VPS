#include "config.h"
#include "protocol.h"
#include "sdl_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>
#include <SDL2/SDL.h>

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
int join_request(int sock, struct sockaddr_in *server_addr)
{
    PLAYER_SIGNALS request = PLAYER_JOIN_REQUEST;
    int bytes_sent = sendto(sock, &request, sizeof(request), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent != (int)sizeof(request)){
        perror("Failed to send the request\n");
        return 1;
    }
    return 0;
}

int main()
{
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

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Init SDL error: %s\n", SDL_GetError());
        return 1;
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

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    while(running){
        int menu = 1;
        int ingame = 0;
        SDL_Rect join_button = { .x = 300, .y = 250, .w = 200, .h = 50 };
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
                            if (join_request(sock, &server_addr) != 0) return 1;
                        }
                    }
                }
            }
            int bytes_received = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);
            if (bytes_received == (int)sizeof(resp) &&
                resp.id != -1 && resp.type == PLAYER_JOIN_ACCEPT){
                menu = 0;
            }
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // Green
            SDL_RenderFillRect(renderer, &join_button);
            SDL_RenderPresent(renderer);
        }
        if (!running) break;
        ingame = 1;
        int id = resp.id;
        server_message serv_mes;
        player_cord all_cord[MAX_PLAYERS];
        player_cord my_cord = {0};

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
            int bytes_received = recvfrom(sock, &serv_mes, sizeof(serv_mes), 0, NULL, NULL);
            if (bytes_received == (int)sizeof(serv_mes)){
                if (serv_mes.type == MSG_POSITIONS) {
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (i != id) {
                            all_cord[i] = serv_mes.positions[i];
                        }
                    }
                    all_cord[id] = my_cord;
                }
                else if (serv_mes.type == MSG_GAME_OVER){
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
            players_rects = player_cords_to_rects(all_cord, MAX_PLAYERS, 50, 50);

            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            render_players(players_rects, MAX_PLAYERS, renderer);
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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(sock);
    return 0;
}
