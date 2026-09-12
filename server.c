#include "config.h"
#include "display.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

int player_join(session_info *session, int server_sock, struct sockaddr_in *client_addr)
{
    if (session == NULL || client_addr == NULL) return 1;
    int index = 0;
    PLAYER_SIGNALS acc = PLAYER_JOIN_DENIED;
    if (session->ready_players >= MAX_PLAYERS){
        printf("Max players\n");
        index = -1;
    }
    if (index != -1){
        while (session->players[index].ready != 0){
            index++;
        }
        acc = PLAYER_JOIN_ACCEPT;
    }

    join_response resp = {
        .status = acc,
        .id = index
    };
    // ANSWER FOR CLIENT
    int bytes_sent = sendto(server_sock, &resp, sizeof(resp), 0,
            (struct  sockaddr *) client_addr, sizeof(*client_addr));
    if (bytes_sent <= 0){
        perror("Failed to send the signal");
        return 1;
    }
    if (index == -1) return 1;

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
            session->cord[id].x++;
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
        server_message mes = {
            .type = MSG_POSITIONS,
            .left_time = session->session_time
        };
        memcpy(mes.positions, session->cord, sizeof(mes.positions));
        int bytes_sent = sendto(sock, &mes, sizeof(mes),
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

    fd_set readfd;
    int retval;
    int nfds = server_sock + 1;
    time_t start_time = time(NULL);
    struct timeval tv;
    while(1){
        time_t now = time(NULL);
        int elapsed = (int)(now - start_time);

        printf("Time: %d sec\n", elapsed);
        if (session.session_time == TIME_FOR_EXIT) break;

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&readfd);
        FD_SET(server_sock, &readfd);
        FD_SET(STDIN_FILENO, &readfd); //for admin

        retval = select(nfds, &readfd, NULL, NULL, &tv);
        if (retval < 0) perror("select() error\n");
        if (FD_ISSET(server_sock, &readfd)){
            move_handle(&session, server_sock, &action);
        }
        session.session_time = elapsed;

    }
    printf("game ended\n");
    close(server_sock);
    return 0;
}

