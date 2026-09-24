#include "config.h"
#include "display.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>


static int same_player(const struct sockaddr_in *left,
                       const struct sockaddr_in *right)
{
    return left->sin_family == right->sin_family &&
           left->sin_port == right->sin_port &&
           left->sin_addr.s_addr == right->sin_addr.s_addr;
}

static int send_server_message(int sock, const player_info *player,
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

int player_join(session_info *session, int server_sock,
                const struct sockaddr_in *client_addr)
{
    if (session == NULL || client_addr == NULL) return 1;

    int index = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!session->players[i].ready) {
            index = i;
            break;
        }
    }

    server_message response = {
        .type = index >= 0 ? PLAYER_JOIN_ACCEPT : PLAYER_JOIN_DENIED,
        .id = index,
    };
    session->players_client[index].color.R = rand() % 256;
    session->players_client[index].color.G = rand() % 256;
    session->players_client[index].color.B = rand() % 256;
    session->players_client[index].cord.x = 10;
    session->players_client[index].cord.y = -10 - index * 60;
    session->players_client[index].ingame = 1;
    memcpy(response.players, session->players_client, sizeof(response.players));
    if (sendto(server_sock, &response, sizeof(response), 0,
               (const struct sockaddr *)client_addr, sizeof(*client_addr)) !=
        (ssize_t)sizeof(response)) {
        perror("Failed to send join response\n");
        return 1;
    }

    if (index < 0) {
        printf("Max players\n");
        return 1;
    }

    session->players[index].ready = 1;
    session->players[index].player_addr = *client_addr;
    session->ready_players++;

    server_message new_player_mes = {
        .type = MSG_NEW_PLAYER,
        .players_in_lobby = session->ready_players,
        .id = index,
    };
    memcpy(new_player_mes.players, session->players_client, sizeof(new_player_mes.players));

    for (int i = 0; i < MAX_PLAYERS; i++){
        if (!session->players[i].ready) continue;
        if (send_server_message(server_sock, &session->players[i], &new_player_mes) != 0){
            fprintf(stderr, "Send message MSG_NEW_PLAYER for id: %d error\n",i);
        }
    }

    printf("New player:\n");
    print_player(session, index);
    return 0;
}

int wait_players(session_info *session, int server_sock)
{
    while (session->ready_players < PLAYERS_TO_START) {
        PLAYER_SIGNALS request;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        ssize_t bytes_received = recvfrom(server_sock, &request, sizeof(request), 0,
                                          (struct sockaddr *)&client_addr,
                                          &client_addr_len);
        if (bytes_received < 0) {
            if (errno == EINTR) continue;
            perror("receive join request");
            continue;
        }
        if (bytes_received != (ssize_t)sizeof(request) ||
            request != PLAYER_JOIN_REQUEST) {
            continue;
        }
        player_join(session, server_sock, &client_addr);
    }
    for (int i = 0; i < session->ready_players; i++){
        server_message mes_start= {
            .type = MSG_GAME_START,
            .left_time = TIME_FOR_EXIT,
        };
        if (send_server_message(server_sock, session->players + i, &mes_start) == 1)
            fprintf(stderr, "Failed send message to start fot player: %d\n",i);
    }
    return 0;
}

/* Handle the new player_message protocol: the client sends its complete
 * position in player_message.cord instead of a key enum. */
int move_handle(session_info *session, int sock)
{
    player_message message;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    ssize_t bytes_received = recvfrom(sock, &message, sizeof(message), 0,
                                      (struct sockaddr *)&client_addr,
                                      &client_addr_len);
    if (bytes_received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) perror("receive move");
        return 1;
    }
    if (bytes_received != (ssize_t)sizeof(message) || message.type != SEND_CORD) {
        return 1;
    }

    int id = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready &&
            same_player(&session->players[i].player_addr, &client_addr)) {
            id = i;
            break;
        }
    }
    if (id < 0) return 1; /* Ignore packets from unknown clients. */

    session->players_client[id].cord = message.cord;
    print_player_pos(session, id);

    server_message response = {
        .type = MSG_CLIENTS_INFO,
        .left_time = session->session_time
    };
    memcpy(response.players, session->players_client, sizeof(response.players));

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready) {
            send_server_message(sock, &session->players[i], &response);
        }
    }
    return 0;
}

void session_end(session_info *session, int sock)
{
    session->session_number++;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!session->players[i].ready) continue;
        server_message message = {
            .type = MSG_GAME_OVER,
            .left_time = session->session_time
        };
        send_server_message(sock, &session->players[i], &message);
    }

    memset(session->players_client, 0, sizeof(session->players_client));
    memset(session->players, 0, sizeof(session->players));
    session->ready_players = 0;
    session->session_time = 0;
}

int main(void)
{
    srand(time(NULL)); // for random color
    session_info session = {
        .session_number = 1,
        .ready_players = 0,
        .session_time = 0
    };

    int server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock < 0) {
        perror("Socket create error");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind error");
        close(server_sock);
        return 1;
    }
    printf("Successfully bound\n");

    while (1) {
        wait_players(&session, server_sock);
        printf("GAME START\n");

        time_t start_time = time(NULL);
        while (1) {
            time_t now = time(NULL);
            session.session_time = (int)(now - start_time);
            if (session.session_time >= TIME_FOR_EXIT) break;

            fd_set readfd;
            FD_ZERO(&readfd);
            FD_SET(server_sock, &readfd);
            struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

            int result = select(server_sock + 1, &readfd, NULL, NULL, &timeout);
            if (result < 0) {
                if (errno == EINTR) continue;
                perror("select error");
                break;
            }
            if (result > 0 && FD_ISSET(server_sock, &readfd)) {
                move_handle(&session, server_sock);
            }
        }

        printf("game ended\n");
        session_end(&session, server_sock);
        printf("Session number: %d\nWait new player...\n", session.session_number);
    }

    close(server_sock);
    return 0;
}
