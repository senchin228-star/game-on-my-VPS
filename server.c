#include "config.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
void print_player_pos(session_info  *session, int id)
{
    if (session == NULL){
        printf("NULL session\n");
        return;
    }
    int x, y;
    x = session->cord[id].x;
    y = session->cord[id].y;
    printf("ID: %d X: %d, Y: %d\n", id, x, y);
}

void print_player(session_info *session, int id)
{
    if (session == NULL){
        printf("NULL session\n");
        return;
    }
    if (session->players[id].ready == 0){
        printf("Player not ready");
        return;
    }
    char player_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &session->players[id].player_addr.sin_addr,
                        player_ip, INET_ADDRSTRLEN);
    int player_port = ntohs(session->players[id].player_addr.sin_port);
    printf("ID: %d IP: %s PORT: %d\n", id, player_ip, player_port);
    return;
}

void print_players(session_info *session)
{
    if (session == NULL){
        printf("NULL session\n");
        return;
    }
    if (session->ready_players == 0){
        printf("No one player\n");
        return;
    }
    printf("-------Players-------\n");
    for (int i = 0; i < MAX_PLAYERS; i++){
        if (!session->players[i].ready) continue;
        char player_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &session->players[i].player_addr.sin_addr,
                            player_ip, INET_ADDRSTRLEN);
        int player_port = ntohs(session->players[i].player_addr.sin_port);
        printf("\nID: %d IP: %s PORT: %d\n", i, player_ip, player_port);
    }
    printf("---------------------\n");
    return;
}


int player_join(session_info *session, int server_sock, struct sockaddr_in *client_addr)
{
    if (session == NULL || client_addr == NULL) return 1;
    PLAYER_SIGNALS acc = PLAYER_JOIN_ACCEPT;
    if (session->ready_players >= MAX_PLAYERS){
        acc = PLAYER_JOIN_DENIED;
        printf("Max players\n");
    }
    // ANSWER FOR CLIENT
    int bytes_sent = sendto(server_sock, &acc, sizeof(PLAYER_SIGNALS), 0,
            (struct  sockaddr *) client_addr, sizeof(*client_addr));
    if (bytes_sent <= 0){
        perror("Failed to send the signal");
        return 1;
    }
    if (session->ready_players >= MAX_PLAYERS) return 1;

    int index = 0;
    while (session->players[index].ready != 0){
        index++;
    }
    session->players[index].ready = 1;
    session->players[index].player_addr = *client_addr;
    session->ready_players++;
    session->cord[index].id = index;

    printf("New player:\n");
    print_player(session, index);
    return 0;
}

int wait_players(session_info *session, int server_sock, PLAYER_SIGNALS *action,
                struct sockaddr_in *client_addr, socklen_t *client_addr_len)
{
    while(1){
        if (session->ready_players == PLAYERS_TO_START) return 0;
        *client_addr_len = sizeof(*client_addr);
        int bytes_received = recvfrom(server_sock, action, sizeof(PLAYER_SIGNALS), 0,
                (struct sockaddr*) client_addr, client_addr_len);
        if (bytes_received <= 0){
            perror("receive error");
            continue;
        }
        if (*action == PLAYER_JOIN_REQUEST) player_join(session, server_sock, client_addr);
        else printf("NE TOT SIGNAL"); //otladka
    }
    return 1;
}

int move_handle(session_info *session, int sock, PLAYER_SIGNALS *action)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int bytes_received = recvfrom(sock, action, sizeof(PLAYER_SIGNALS), 0,
                (struct sockaddr*) &client_addr, &client_addr_len);
    if (bytes_received <= 0){
        perror("receive error");
        return 1;
    }
    int id = 0;
    char find = 0;
    for (; id < MAX_PLAYERS; id++){
        if (session->players[id].player_addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                session->players[id].player_addr.sin_port == client_addr.sin_port) {
            find = 1;
            break;
        }
    }
    if (!find) return 1; // UNKNOWN PLAYER
    switch(*action){
        case LEFT_KEY:
            session->cord[id].x--; // x = [0] ; y =[1]
            break;
        case RIGHT_KEY:
            session->cord[id].x++; // x = [0] ; y =[1]
            break;
        case DOWN_KEY:
            session->cord[id].y--;
            break;
        case UP_KEY:
            session->cord[id].y++;
            break;
        default:
            break;
    }
    print_player_pos(session, id);
    id = 0;
    for (;id < MAX_PLAYERS; id++){
        player_info *player = &session->players[id];
        if (!player->ready) continue;
        int bytes_sent = sendto(sock, session->cord, sizeof(session->cord),
                            0,(struct  sockaddr *) &player->player_addr, sizeof(player->player_addr));
        if (bytes_sent <= 0) perror("Failed to send cord\n");
    }
    return 0;
}

int main()
{
    session_info session = {
        .session_number = 1,
        .players = {{{0}}},
        .ready_players = 0,
        .session_time = 0
    };

    int server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock < 0){
        perror("Socket create ERR");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0){
        perror("bind error");
        return 1;
    }
    printf("Successfully bound\n");

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    PLAYER_SIGNALS action;
    wait_players(&session, server_sock, &action, &client_addr, &client_addr_len);
    printf("GAME START\n");
    while(1){
        move_handle(&session, server_sock, &action);
    }
    return 0;
}

