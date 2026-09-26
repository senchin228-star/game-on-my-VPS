#include "config.h"
#include "server_utils.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>


static void broadcast_lobby_status(session_info *session, int sock, int left_time)
{
    server_message message = {
        .type = MSG_LOBBY_INFO,
        .left_time = left_time,
        .players_in_lobby = session->ready_players
    };
    memcpy(message.players, session->players_client, sizeof(message.players));

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready) {
            send_server_message(sock, &session->players[i], &message);
        }
    }
}

static void broadcast_game_start(session_info *session, int sock)
{
    server_message message = {
        .type = MSG_GAME_START,
        .left_time = 0
    };

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready) {
            send_server_message(sock, &session->players[i], &message);
        }
    }
}

static int countdown(session_info *session, int server_sock)
{
    double start_time = monotonic_seconds();
    int previous_left_time = -1;

    while (1) {
        double now = monotonic_seconds();
        double elapsed = now - start_time;

        int left_time = COUNTDOWN_SECONDS - (int)elapsed;

        if (left_time < 0) {
            left_time = 0;
        }

        //send when time changed
        if (left_time != previous_left_time) {
            broadcast_lobby_status(session, server_sock, left_time);
            previous_left_time = left_time;

            printf("Game starts in: %d\n", left_time);
        }

        if (elapsed >= COUNTDOWN_SECONDS) {
            broadcast_game_start(session, server_sock);
            return 0;
        }

        // wait udp-packet max 100ms
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_sock, &readfds);

        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 100000
        };

        int result = select(
            server_sock + 1,
            &readfds,
            NULL,
            NULL,
            &timeout
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("select during countdown");
            return 1;
        }

        // HANDLE PLAYER SIGNAL
        if (result > 0 && FD_ISSET(server_sock, &readfds)) {
            player_message message;
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);

            ssize_t bytes_received = recvfrom(
                server_sock,
                &message,
                sizeof(message),
                0,
                (struct sockaddr *)&client_addr,
                &client_addr_len
            );

            if (bytes_received < 0) {
                if (errno == EINTR || errno == EAGAIN ||
                    errno == EWOULDBLOCK) {
                    continue;
                }

                perror("recvfrom during countdown");
                continue;
            }
            // HANDLE PLAYER SIGNALS
        }
    }
}

int player_join(session_info *session, int server_sock,
                const struct sockaddr_in *client_addr, char* nick)
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
    if (index >= 0){
        session->players_client[index].color.R = rand() % 256;
        session->players_client[index].color.G = rand() % 256;
        session->players_client[index].color.B = rand() % 256;
        session->players_client[index].cord.x = 100;
        session->players_client[index].cord.y = -150 - index * 100;
        session->players_client[index].ingame = 1;
        printf("Nick: %s\n", nick);
        snprintf(session->players_client[index].nickname,
                sizeof(session->players_client[index].nickname),"%s", nick);
    }
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
    int left_time = 0;
    broadcast_lobby_status(session, server_sock, left_time);

    printf("New player:\n");
    print_player(session, index);
    return 0;
}

int wait_players(session_info *session, int server_sock)
{
    while (session->ready_players < PLAYERS_TO_START) {
        player_message request;
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
            request.type != PLAYER_JOIN_REQUEST /*|| request.nickname == NULL*/) {
            continue;
        }
        player_join(session, server_sock, &client_addr, request.nickname);
    }
    countdown(session, server_sock);
    broadcast_game_start(session, server_sock);
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
void run_game(session_info *session, int server_sock, size_t start_time)
{
    while(1){
        time_t now = time(NULL);
        session->session_time = (int)(now - start_time);
        if (session->session_time >= TIME_FOR_EXIT) break;

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
            move_handle(session, server_sock);
        }
    }
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

        time_t start_time = time(NULL); //for random color
        run_game(&session, server_sock, start_time);

        printf("game ended\n");
        session_end(&session, server_sock);
        printf("Session number: %d\nWait new player...\n", session.session_number);
    }

    close(server_sock);
    return 0;
}
