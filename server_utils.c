#include "server_utils.h"
#include "protocol.h"
#include <stddef.h>
#include <stdio.h>
#include <time.h>
double monotonic_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec +
           (double)ts.tv_nsec / 1000000000.0;
}

int same_player(const struct sockaddr_in *left,
                       const struct sockaddr_in *right)
{
    return left->sin_family == right->sin_family &&
           left->sin_port == right->sin_port &&
           left->sin_addr.s_addr == right->sin_addr.s_addr;
}

int send_server_message(int sock, const player_info *player,
                               const server_message *message)
{
    ssize_t bytes_sent = sendto(sock, message, sizeof(*message), 0,
                                (const struct sockaddr *)&player->player_addr,
                                sizeof(player->player_addr));
    if (bytes_sent != (ssize_t)sizeof(*message)) {
        perror("Failed to send server message");
        return 1;
    }
    return 0;
}
void print_player_pos(session_info  *session, int id)
{
    if (session == NULL){
        printf("NULL session\n");
        return;
    }
    int x, y;
    x = session->players_client[id].cord.x;
    y = session->players_client[id].cord.y;
    printf("Nick: %s ID: %d X: %d, Y: %d\n",session->players_client[id].nickname,
                                             id, x, y);
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
    printf("Nickname: %s\n", session->players_client[id].nickname);
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
