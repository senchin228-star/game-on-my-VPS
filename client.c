#include "config.h"
#include "protocol.h"
#include "sdl_utils.h"
#include "text_utils.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

int leave(int sock, struct sockaddr_in *server_addr)
{
    player_message mes = {
        .type = PLAYER_LEAVE,
    };
    int bytes_sent = sendto(sock, &mes, sizeof(mes), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent != (int)sizeof(mes)){
        perror("Failed to send the mes\n");
        return 1;
    }
    return 0;
}

int send_message(int sock, struct sockaddr_in *server_addr, player_message *mes)
{
    int bytes_sent = sendto(sock, mes, sizeof(*mes), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent != (int)sizeof(*mes)){
        perror("Failed to send the mes\n");
        return 1;
    }
    return 0;
}

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

static int save_reconnect_token(uint64_t token)
{
    int fd = open("client_runtime.conf", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return 1;
    if (fchmod(fd, 0600) < 0) {
        close(fd);
        return 1;
    }

    FILE *config = fdopen(fd, "w");
    if (config == NULL) {
        close(fd);
        return 1;
    }

    int write_failed = fprintf(config, "reconnect_token=%" PRIu64 "\n", token) < 0;
    if (fflush(config) != 0) write_failed = 1;
    if (fclose(config) != 0) write_failed = 1;
    return write_failed;
}

static uint64_t load_reconnect_token(void)
{
    FILE *config = fopen("client_runtime.conf", "r");
    if (config == NULL) return 0;

    uint64_t token = 0;
    if (fscanf(config, "reconnect_token=%" SCNu64, &token) != 1) {
        token = 0;
    }
    fclose(config);
    return token;
}

static int reconnect_request(int sock,
                             struct sockaddr_in *server_addr,
                             uint64_t token)
{
    player_message request = {
        .type = PLAYER_RECONNECT_REQUEST,
        .reconnect_token = token
    };
    return send_message(sock, server_addr, &request);
}

static void append_nickname(char *nickname, size_t capacity, const char *input,
                            size_t input_capacity)
{
    size_t nickname_length = strnlen(nickname, capacity);
    size_t input_length = strnlen(input, input_capacity);
    if (nickname_length >= capacity - 1) return;

    size_t available = capacity - nickname_length - 1;
    size_t copy_length = input_length < available ? input_length : available;
    // Back up if truncation would leave only part of a UTF-8 character.
    while (copy_length > 0 &&
           (((unsigned char)input[copy_length] & 0xC0) == 0x80)) {
        copy_length--;
    }

    memcpy(nickname + nickname_length, input, copy_length);
    nickname[nickname_length + copy_length] = '\0';
}

static void remove_last_nickname_character(char *nickname)
{
    size_t length = strlen(nickname);
    if (length == 0) return;

    // Walk back over UTF-8 continuation bytes to remove the whole character.
    do {
        length--;
    } while (length > 0 &&
             (((unsigned char)nickname[length] & 0xC0) == 0x80));
    nickname[length] = '\0';
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("Init SDL error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        printf("Init TTF error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

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
    SDL_Color blue = {30, 144, 255, 255};
    SDL_Color another_blue = {30, 133, 255, 255};


    if (IMG_Init(IMG_INIT_PNG) == 0){
        fprintf(stderr, "Error SDL2_image Initialization: %s\n", IMG_GetError());
    }

    SDL_Window* window = SDL_CreateWindow(
        "game-on-my-VPS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
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
    SDL_Rect nick_button = { .x = 550, .y = 150, .w = 200, .h = 40 };

    while(running){
        int menu = 1;
        int ingame = 0;
        int lobby = 0;
        int reconnecting = 0;
        int reconnect_finished = 0;
        uint64_t saved_token = load_reconnect_token();
        // Keep the input buffer aligned with the nickname field on the wire.
        char nickname[sizeof(((player_message *)0)->nickname)] = {0};
        int nickname_focused = 1;
        SDL_StartTextInput();
        while(menu){
            while (SDL_PollEvent(&event)){
                if (event.type == SDL_QUIT){
                    if (players_rects != NULL) free(players_rects);
                    running = 0;
                    menu = 0;
                    break;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN){
                    if (event.button.button == SDL_BUTTON_LEFT){
                        SDL_Point mouse_pos = {.x = event.button.x, .y = event.button.y};
                        if (SDL_PointInRect(&mouse_pos, &nick_button)) {
                            nickname_focused = 1;
                            SDL_StartTextInput();
                        } else if (SDL_PointInRect(&mouse_pos, &join_button) &&
                                   (nickname[0] != '\0' || saved_token != 0)) {
                            nickname_focused = 0;
                            SDL_StopTextInput();
                            int sent = saved_token != 0
                                ? reconnect_request(sock, &server_addr, saved_token)
                                : join_request(sock, &server_addr, nickname);
                            if (sent != 0) return 1;
                        } else {
                            nickname_focused = 0;
                            SDL_StopTextInput();
                        }
                    }
                }
                else if (event.type == SDL_TEXTINPUT && nickname_focused) {
                    append_nickname(nickname, sizeof(nickname), event.text.text,
                                    sizeof(event.text.text));
                }
                else if (event.type == SDL_KEYDOWN && nickname_focused) {
                    if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        remove_last_nickname_character(nickname);
                    } else if (event.key.keysym.sym == SDLK_RETURN &&
                               (nickname[0] != '\0' || saved_token != 0)) {
                        nickname_focused = 0;
                        SDL_StopTextInput();
                        int sent = saved_token != 0
                            ? reconnect_request(sock, &server_addr, saved_token)
                            : join_request(sock, &server_addr, nickname);
                        if (sent != 0) return 1;
                    }
                }
            }
            int bytes_received = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);
            if (bytes_received == (int)sizeof(resp) &&
                resp.id != -1 && resp.type == PLAYER_JOIN_ACCEPT){
                menu = 0;
                lobby = 1;
            } else if (bytes_received == (int)sizeof(resp) &&
                       resp.type == MSG_RECONNECT_ACCEPT) {
                reconnecting = 1;
                menu = 0;
                ingame = 1;
            } else if (bytes_received == (int)sizeof(resp) &&
                       resp.type == MSG_GAME_OVER) {
                reconnect_finished = 1;
                menu = 0;
                unlink("client_runtime.conf");
            } else if (bytes_received == (int)sizeof(resp) &&
                       resp.type == MSG_RECONNECT_DENIED) {
                unlink("client_runtime.conf");
                saved_token = 0;
                if (join_request(sock, &server_addr, nickname) != 0) return 1;
            }
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255);
            SDL_RenderClear(renderer);
            if (connect_but_tex != NULL){
                SDL_RenderCopy(renderer, connect_but_tex, NULL, &join_button);
            }
            else{
                SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
                SDL_RenderFillRect(renderer, &join_button);
            }
            SDL_SetRenderDrawColor(renderer, 30, 133, 255, 255);
            SDL_RenderFillRect(renderer, &nick_button);

            render_text_with_bg(renderer, font_medium, "Enter your nickname:", 180, 150, white, blue);
            if (nickname[0] != '\0'){
                render_text_with_bg(renderer, font_medium, nickname, 550, 150, white, another_blue);
            }
            SDL_RenderPresent(renderer);

        }
        if (!running) break;
        int id = resp.id;
        player_client_info all_players[MAX_PLAYERS];
        memcpy(all_players, resp.players, sizeof(all_players));
        player_cord my_cord = resp.players[id].cord;
        player_cord apple_cord = {0, 0};
        Uint32 game_start_ticks = 0;
        uint64_t token = 0;
        if (reconnecting) {
            apple_cord = resp.apple_cord;
            game_start_ticks = SDL_GetTicks() - (Uint32)resp.left_time * 1000;
            token = resp.reconnect_token;
        }
        SDL_StopTextInput();

        int countdown_time = -1;
        while (lobby){
            while (SDL_PollEvent(&event)){
                if (event.type == SDL_QUIT){
                    leave(sock, &server_addr);
                    lobby = 0;
                    menu = 0;
                    running = 0;
                    if (players_rects != NULL) {
                        free(players_rects);
                        players_rects = NULL;
                    }
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
                apple_cord = resp.apple_cord;
                game_start_ticks = SDL_GetTicks();
                lobby = 0; 
                ingame = 1;
                token = resp.reconnect_token;
                if (save_reconnect_token(token) != 0) {
                    perror("Failed to save reconnect token");
                }
            }
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255);
            SDL_RenderClear(renderer);
            render_text(renderer, font_large, "WAITING FOR PLAYERS", 100, 50, white);
            char time_text[256] = {0};
            if (countdown_time > 0){
                snprintf(time_text, sizeof(time_text),
                    "Game starts in: %d\n", countdown_time);
                render_text(renderer, font_medium, time_text, 150, 350, yellow);
            }
            players_rects = player_cords_to_rects(
                all_players, MAX_PLAYERS, PLAYER_WIDTH, PLAYER_HEIGHT);
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
        SDL_Rect *apple_rect = NULL;
        int game_finished = reconnect_finished;

        while(ingame){
            if (!running) break;
            frameStart = SDL_GetTicks();
            while(SDL_PollEvent(&event)){
                if (event.type  == SDL_QUIT){
                    if (leave(sock, &server_addr) != 0) {
                        fprintf(stderr, "leave failed\n");
                    }
                    if (players_rects != NULL) {
                        free(players_rects);
                        players_rects = NULL;
                    }
                    running = 0;
                    menu = 0;
                    lobby = 0;
                    ingame = 0;
                    break;
                }
            }
            int bytes_received = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);
            if (bytes_received == (int)sizeof(resp)){
                if (resp.type == MSG_CLIENTS_INFO) {
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        all_players[i] = resp.players[i];
                    }
                    apple_cord = resp.apple_cord;
                    all_players[id].cord = my_cord;
                }
                else if (resp.type == MSG_GAME_OVER){
                    printf("Game over\n");
                    memcpy(all_players, resp.players, sizeof(all_players));
                    ingame = 0;
                    game_finished = 1;
                    unlink("client_runtime.conf");
                }
                else if (resp.type == MSG_GET_APPLE){
                    printf("New apple cord: (%d, %d)\n", resp.apple_cord.x, resp.apple_cord.y);
                    apple_cord.x = resp.apple_cord.x;
                    apple_cord.y = resp.apple_cord.y;
                }
            }
            int elapsed_time = (int)((SDL_GetTicks() - game_start_ticks) / 1000);
            int remaining_time = TIME_FOR_EXIT - elapsed_time;
            if (remaining_time < 0) remaining_time = 0;
            player_cord previous_cord = my_cord;

            const Uint8 *state = SDL_GetKeyboardState(NULL);
            if (state[SDL_SCANCODE_LEFT])  my_cord.x -= PLAYER_SPEED;
            if (state[SDL_SCANCODE_RIGHT]) my_cord.x += PLAYER_SPEED;
            if (state[SDL_SCANCODE_UP])    my_cord.y += PLAYER_SPEED;
            if (state[SDL_SCANCODE_DOWN])  my_cord.y -= PLAYER_SPEED;

            if (my_cord.x < 0) my_cord.x = 0;
            if (my_cord.x > WINDOW_WIDTH - PLAYER_WIDTH) {
                my_cord.x = WINDOW_WIDTH - PLAYER_WIDTH;
            }
            if (my_cord.y > 0) my_cord.y = 0;
            if (my_cord.y < -(WINDOW_HEIGHT - PLAYER_HEIGHT)) {
                my_cord.y = -(WINDOW_HEIGHT - PLAYER_HEIGHT);
            }

            if (my_cord.x < apple_cord.x + PLAYER_WIDTH &&
                my_cord.x + PLAYER_WIDTH > apple_cord.x &&
                my_cord.y < apple_cord.y + PLAYER_HEIGHT &&
                my_cord.y + PLAYER_HEIGHT > apple_cord.y) {
                printf("You got the apple!\n");
                player_message get_point_msg = {
                    .type = GET_POINT,
                    .cord = my_cord
                };
                send_message(sock, &server_addr, &get_point_msg);
            }

            // Keep local movement visible until the next server snapshot arrives.
            all_players[id].cord = my_cord;
            players_rects = player_cords_to_rects(
                all_players, MAX_PLAYERS, PLAYER_WIDTH, PLAYER_HEIGHT);
            apple_rect = apple_cords_to_rects(
                &apple_cord, PLAYER_WIDTH, PLAYER_HEIGHT);

            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255);
            SDL_RenderClear(renderer);
                char time_text[64];
                snprintf(time_text, sizeof(time_text),
                    "Time left: %d", remaining_time);
                render_text(renderer, font_medium, time_text, 20, 20, yellow);
            render_text(renderer, font_medium, "SCORES", 620, 20, white);
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!all_players[i].ingame || all_players[i].nickname[0] == '\0') {
                    continue;
                }

                char score_text[192];
                snprintf(score_text, sizeof(score_text), "%s: %d",
                    all_players[i].nickname, all_players[i].score);
                render_text(renderer, font_small, score_text, 570, 55 + i * 30, white);
            }
            render_players(players_rects, all_players, MAX_PLAYERS, renderer);
            render_player_nicknames(
                players_rects,
                all_players,
                MAX_PLAYERS,
                renderer,
                font_small,
                white
            );
            if (apple_rect) {
                render_apple(apple_rect, renderer);
            }
            SDL_RenderPresent(renderer);

            if (my_cord.x != previous_cord.x ||
                my_cord.y != previous_cord.y) {
                send_move(sock, &server_addr, my_cord);
            }

            if (players_rects != NULL ) free(players_rects);
            if (apple_rect != NULL) free(apple_rect);

            frameTime = SDL_GetTicks() - frameStart;
            if (FRAME_DELAY > frameTime) {
                SDL_Delay(FRAME_DELAY - frameTime);
            }
        }
        if (!running) break;
        while (game_finished && running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = 0;
                    break;
                }
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
                    game_finished = 0;
                    break;
                }
            }
            if (!game_finished || !running) break;

            int highest_score = -1;
            int winner_id = -1;
            int tied = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!all_players[i].ingame) continue;
                if (all_players[i].score > highest_score) {
                    highest_score = all_players[i].score;
                    winner_id = i;
                    tied = 0;
                } else if (all_players[i].score == highest_score) {
                    tied = 1;
                }
            }

            SDL_SetRenderDrawColor(renderer, 18, 35, 48, 255);
            SDL_RenderClear(renderer);
            render_text(renderer, font_large, "GAME OVER", 250, 65, white);
            if (winner_id >= 0 && tied) {
                render_text(renderer, font_medium, "DRAW", 330, 145, yellow);
            } else if (winner_id >= 0) {
                char winner_text[160];
                snprintf(winner_text, sizeof(winner_text), "Winner: %s",
                    all_players[winner_id].nickname);
                render_text(renderer, font_medium, winner_text, 190, 145, yellow);
            } else {
                render_text(renderer, font_medium, "No winner", 300, 145, yellow);
            }

            render_text(renderer, font_medium, "FINAL SCORES", 285, 230, white);
            int score_line = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!all_players[i].ingame || all_players[i].nickname[0] == '\0') {
                    continue;
                }
                char score_text[192];
                snprintf(score_text, sizeof(score_text), "%s: %d",
                    all_players[i].nickname, all_players[i].score);
                render_text(renderer, font_small, score_text, 280,
                    285 + score_line * 35, white);
                score_line++;
            }
            render_text(renderer, font_small, "Press any key to continue", 240, 520, yellow);
            SDL_RenderPresent(renderer);
            SDL_Delay(16);
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
