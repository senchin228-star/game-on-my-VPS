#include "config.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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
    for (int i = 0; i < MAX_PLAYERS; i++){
        char player_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &session->players[i].player_addr.sin_addr,
                            player_ip, INET_ADDRSTRLEN);
        int player_port = ntohs(session->players[i].player_addr.sin_port);
        printf("ID: %d IP: %s PORT: %d\n", i, player_ip, player_port);
    }
    return;
}


int player_join(session_info *session, struct sockaddr_in *client_addr)
{
    if (session == NULL || client_addr == NULL) return 1;
    if (session->ready_players >= MAX_PLAYERS){
        printf("Max players\n");
        return 1;
    } 
    int index = 0;
    while (session->players[index].ready != 0){
        index++;
    }
    session->players[index].ready = 1;
    session->players[index].player_addr = *client_addr;
    session->ready_players++;
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
    for (int i = 0; i <= MAX_PLAYERS; i++){
        session.players[i].ready = 0;
    }
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

    int action;
    while(1){
        print_players(&session);
        int bytes_received = recvfrom(server_sock, &action, sizeof(int), 0,
                (struct sockaddr*) &client_addr, &client_addr_len);
        if (bytes_received <= 0){
            perror("receive error");
            continue;
        }
        player_join(&session, &client_addr);
    }
    return 0;
}


