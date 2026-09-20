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

int send_move(int sock, struct sockaddr_in *server_addr, PLAYER_SIGNALS sig)
{
    int signal = sig;
    int bytes_sent = sendto(sock, &signal, sizeof(int), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent <= 0){
        perror("Failed to send the signal\n");
        return 1;
    }
    return 0;
}
int join_request(int sock, struct sockaddr_in *server_addr)
{
    PLAYER_SIGNALS request = PLAYER_JOIN_REQUEST;
    int bytes_sent = sendto(sock, &request, sizeof(int), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent <= 0){
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
            join_response resp;
            int bytes_received = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);
            if (bytes_received > 0 && resp.id != -1){
                menu = 0;
            }
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // Green
            SDL_RenderFillRect(renderer, &join_button);
            SDL_RenderPresent(renderer);
        }
        ingame = 1;

        server_message serv_mes;
        player_cord all_cord[MAX_PLAYERS];
        memset(all_cord, 0, sizeof(all_cord));

        while(ingame){
            while(SDL_PollEvent(&event)){
                if (event.type  == SDL_QUIT){
                    running = 0;
                    ingame = 0;
                    break;
                }
            }
            int bytes_received = recvfrom(sock, &serv_mes, sizeof(serv_mes), 0, NULL, NULL);
            if (bytes_received > 0){
                if (serv_mes.type == MSG_POSITIONS)
                    memcpy(all_cord, serv_mes.positions, sizeof(all_cord));
                else if (serv_mes.type == MSG_GAME_OVER){
                    printf("Game over\n");
                    ingame = 0;
                }
            }
            players_rects = player_cords_to_rects(all_cord, MAX_PLAYERS, 50, 50);

            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            render_players(players_rects, MAX_PLAYERS, renderer);
            SDL_RenderPresent(renderer);
            if (players_rects != NULL ) free(players_rects);

            const Uint8 *state = SDL_GetKeyboardState(NULL);
            if (state[SDL_SCANCODE_LEFT])  { send_move(sock, &server_addr, LEFT_KEY); }
            if (state[SDL_SCANCODE_RIGHT]) { send_move(sock, &server_addr, RIGHT_KEY); }
            if (state[SDL_SCANCODE_UP])    { send_move(sock, &server_addr, UP_KEY); }
            if (state[SDL_SCANCODE_DOWN])  { send_move(sock, &server_addr, DOWN_KEY); }
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(sock);
    return 0;
}
