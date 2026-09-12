#include "display.h"
#include "protocol.h"
#include <stddef.h>
#include <stdio.h>
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
