#include "config.h"
#include "protocol.h"
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

    while(running){
        int menu = 1;
        SDL_Rect join_button = { .x = 300, .y = 250, .w = 200, .h = 50 };
        while(menu){
            while (SDL_PollEvent(&event)){
                if (event.type == SDL_QUIT){
                    running = 0;
                    menu = 0;
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
            int bytes_received = recvfrom(sock, &resp, sizeof(join_response), 0, NULL, NULL);
            if (bytes_received > 0 && resp.id != -1){
                menu = 0;
            }
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // blue
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // Green
            SDL_RenderFillRect(renderer, &join_button);
            SDL_RenderPresent(renderer);
        }
        running = 0;
/*
        server_message serv_mes;
        player_cord all_cord[MAX_PLAYERS];
        fd_set readfd;
        int retval;
        int nfds = sock + 1;
        while(1)
        {
            FD_ZERO(&readfd);
            FD_SET(STDIN_FILENO, &readfd);
            FD_SET(sock, &readfd);
            retval = select(nfds, &readfd ,NULL, NULL, NULL);
            if (retval == -1) perror("Select() error");
            else if (FD_ISSET(STDIN_FILENO, &readfd)){
               PLAYER_SIGNALS move = get_move();
               send_move(sock, &server_addr, move);
            }
            if (FD_ISSET(sock, &readfd)){
                printf("Get new pos\n");
                int bytes_received = recvfrom(sock, &serv_mes, sizeof(serv_mes), 0, NULL, NULL);
                if (bytes_received <= 0) perror("Failed get server message\n");
                if (serv_mes.type == MSG_POSITIONS)
                    memcpy(all_cord, serv_mes.positions, sizeof(all_cord));
                else if (serv_mes.type == MSG_GAME_OVER){
                    printf("Game over\n");
                    break;
                }
                else (printf("Get another server message type\n"));
            }
            
        }
        printf("Press Y to play again or any other key to exit\n");
        int choice = fgetc(stdin);
        int c;
        while ((c = fgetc(stdin)) != '\n' && c != EOF){}
        if (tolower((unsigned char)choice) == 'y') break;
    }*/
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(sock);
    return 0;
}
