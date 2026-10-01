#include "config.h"
#include "server_utils.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>

static void apply_server_settings(server_message *message)
{
    message->player_speed = PLAYER_SPEED;
    message->max_players = MAX_PLAYERS;
    message->players_to_start = PLAYERS_TO_START;
    message->round_duration = TIME_FOR_EXIT;
    message->window_width = WINDOW_WIDTH;
    message->window_height = WINDOW_HEIGHT;
    message->player_width = PLAYER_WIDTH;
    message->player_height = PLAYER_HEIGHT;
}

static void broadcast_lobby_status(session_info *session, int sock, int left_time)
{
    server_message message = {
        .type = MSG_LOBBY_INFO,
        .left_time = left_time,
        .players_in_lobby = session->ready_players
    };
    apply_server_settings(&message);
    memcpy(message.players, session->players_client,
           sizeof(session->players_client));

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready) {
            message.id = i;
            send_server_message(sock, &session->players[i], &message);
        }
    }
}

static void broadcast_game_start(session_info *session, int sock)
{
    server_message message = {
        .type = MSG_GAME_START,
        .left_time = 0,
        .apple_cord = session->apple_cord
    };
    apply_server_settings(&message);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!session->players[i].ready) continue;
        if (create_token(&session->players[i].reconnect_token) != 0) {
            perror("Failed to create reconnect token");
            continue;
        }
        // Each player receives the token stored in their own session slot.
        message.reconnect_token = session->players[i].reconnect_token;
        send_server_message(sock, &session->players[i], &message);
    }
}

static int find_token_player(const session_info *session, uint64_t token)
{
    if (session == NULL || token == 0) return -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready &&
            session->players[i].reconnect_token == token) {
            return i;
        }
    }
    return -1;
}

static void answer_reconnect(session_info *active_session,
                             const session_info *finished_session,
                             int sock,
                             const struct sockaddr_in *client_addr,
                             uint64_t token)
{
    server_message response = {0};
    apply_server_settings(&response);
    player_info recipient = { .player_addr = *client_addr };
    int id = find_token_player(active_session, token);

    if (id >= 0 && active_session->session_time < TIME_FOR_EXIT) {
        active_session->players[id].player_addr = *client_addr;
        response.type = MSG_RECONNECT_ACCEPT;
        response.id = id;
        response.left_time = active_session->session_time;
        response.apple_cord = active_session->apple_cord;
        response.reconnect_token = token;
        memcpy(response.players, active_session->players_client,
             sizeof(active_session->players_client));
    } else {
        // A token from the last finished game gets its final score, not a live reconnect.
        id = find_token_player(finished_session, token);
        if (id >= 0) {
            response.type = MSG_GAME_OVER;
            response.id = id;
            response.left_time = finished_session->session_time;
                 memcpy(response.players, finished_session->players_client,
                     sizeof(finished_session->players_client));
        } else {
            response.type = MSG_RECONNECT_DENIED;
        }
    }

    send_server_message(sock, &recipient, &response);
}

static int countdown(session_info *session,
                     const session_info *finished_session,
                     int server_sock)
{
    // Use a monotonic clock so wall-clock adjustments cannot change the countdown.
    double start_time = monotonic_seconds();
    int previous_left_time = -1;

    while (1) {
        double now = monotonic_seconds();
        double elapsed = now - start_time;

        int left_time = COUNTDOWN_SECONDS - (int)elapsed;

        if (left_time < 0) {
            left_time = 0;
        }

        // Broadcast once per displayed second while the loop also services UDP traffic.
        if (left_time != previous_left_time) {
            broadcast_lobby_status(session, server_sock, left_time);
            previous_left_time = left_time;

            printf("Game starts in: %d\n", left_time);
        }

        if (elapsed >= COUNTDOWN_SECONDS) {
            return 0;
        }

        // Short select intervals keep player requests responsive during the countdown.
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
            if (bytes_received == (ssize_t)sizeof(message) &&
                message.type == PLAYER_RECONNECT_REQUEST) {
                answer_reconnect(NULL, finished_session, server_sock,
                                 &client_addr, message.reconnect_token);
                continue;
            }
            int id = -1;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (session->players[i].ready &&
                    same_player(&session->players[i].player_addr, &client_addr)) {
                    id = i;
                    continue;
                }
            }
            // During the lobby, accept control messages only from registered endpoints.
            if (id < 0) continue;

            
            if (bytes_received == (ssize_t)sizeof(message) &&
                message.type == PLAYER_LEAVE) {
                if (id >= 0) {
                    session->ready_players--;
                    memset(&session->players[id], 0, sizeof(session->players[id]));
                    memset(&session->players_client[id], 0, sizeof(session->players_client[id]));
                    left_time = 0;
                    broadcast_lobby_status(session, server_sock, left_time);
                    return 1;
                }
            continue;
            }
            if (bytes_received == (ssize_t)sizeof(message) &&
                message.type == PLAYER_LEAVE) {
                    session->players[id].ready = 0;
            }
        }
    }
}

int player_join(session_info *session, int server_sock,
                const struct sockaddr_in *client_addr, char* nick)
{
    if (session == NULL || client_addr == NULL) return 1;

    int index = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready &&
            same_player(&session->players[i].player_addr, client_addr)) {
            index = i;
            break;
        }
        if (index < 0 && !session->players[i].ready) {
            index = i;
        }
    }

    int already_joined = index >= 0 && session->players[index].ready;

    server_message response = {
        .type = index >= 0 ? PLAYER_JOIN_ACCEPT : PLAYER_JOIN_DENIED,
        .id = index,
    };
    apply_server_settings(&response);
    if (index >= 0 && !already_joined){
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
        memcpy(response.players, session->players_client,
            sizeof(session->players_client));

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

    if (already_joined) {
        return 0;
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

int wait_players(session_info *session,
                 const session_info *finished_session,
                 int server_sock)
{
    while (1) {
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
        if (bytes_received != (ssize_t)sizeof(request)) {
            continue;
        }
        if (request.type == PLAYER_RECONNECT_REQUEST) {
            answer_reconnect(NULL, finished_session, server_sock,
                             &client_addr, request.reconnect_token);
            continue;
        }
        if (request.type != PLAYER_JOIN_REQUEST && request.type != PLAYER_LEAVE) {
            continue;
        }
        if (request.type == PLAYER_LEAVE) {
            int id = -1;
            for (int i = 0; i < MAX_PLAYERS; i++) {
            if (session->players[i].ready &&
                same_player(&session->players[i].player_addr, &client_addr)) {
                id = i;
                break;
            }
            }
            if (id >= 0) {
            session->ready_players--;
            memset(&session->players[id], 0, sizeof(session->players[id]));
            memset(&session->players_client[id], 0, sizeof(session->players_client[id]));
            }
            continue;
        }
        player_join(session, server_sock, &client_addr, request.nickname);
        if (session->ready_players >= PLAYERS_TO_START &&
            countdown(session, finished_session, server_sock) == 0) break;
    }
    session->apple_cord.x = rand() % (WINDOW_WIDTH - PLAYER_WIDTH);
    session->apple_cord.y = -(rand() % (WINDOW_HEIGHT - PLAYER_HEIGHT));
    broadcast_game_start(session, server_sock);
    return 0;
}

/* Handle the new player_message protocol: the client sends its complete
 * position in player_message.cord instead of a key enum. */
int action_handle(session_info *session,
                  const session_info *finished_session,
                  int sock)
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
    if (bytes_received != (ssize_t)sizeof(message)) {
        return 1;
    }

    if (message.type == PLAYER_RECONNECT_REQUEST) {
        answer_reconnect(session, finished_session, sock, &client_addr,
                         message.reconnect_token);
        return 0;
    }

    int id = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready &&
            same_player(&session->players[i].player_addr, &client_addr)) {
            id = i;
            break;
        }
    }
    // UDP source address and port identify the player; nicknames are not identities.
    if (id < 0) return 1;

    if (message.type == SEND_CORD) {
        session->players_client[id].cord = message.cord;
        if (session->players_client[id].cord.x < 0) {
            session->players_client[id].cord.x = 0;
        }
        if (session->players_client[id].cord.x > WINDOW_WIDTH - PLAYER_WIDTH) {
            session->players_client[id].cord.x = WINDOW_WIDTH - PLAYER_WIDTH;
        }
        if (session->players_client[id].cord.y > 0) {
            session->players_client[id].cord.y = 0;
        }
        if (session->players_client[id].cord.y < -(WINDOW_HEIGHT - PLAYER_HEIGHT)) {
            session->players_client[id].cord.y = -(WINDOW_HEIGHT - PLAYER_HEIGHT);
        }
    }

    // Validate the overlap on the server instead of trusting the client's point request.
    if (message.type == GET_POINT && session->players_client[id].cord.x < session->apple_cord.x + PLAYER_WIDTH &&
            session->players_client[id].cord.x + PLAYER_WIDTH > session->apple_cord.x &&
            session->players_client[id].cord.y < session->apple_cord.y + PLAYER_HEIGHT &&
            session->players_client[id].cord.y + PLAYER_HEIGHT > session->apple_cord.y) {

        session->players_client[id].score += 1;
        printf("Player %d got the apple!\n", id);
        printf("Player %d's score: %d\n", id, session->players_client[id].score);
        session->apple_cord.x = rand() % (WINDOW_WIDTH - PLAYER_WIDTH);
        session->apple_cord.y = -(rand() % (WINDOW_HEIGHT - PLAYER_HEIGHT));
        server_message apple_message = {
            .type = MSG_GET_APPLE,
            .apple_cord = session->apple_cord
        };
        apply_server_settings(&apple_message);
        send_server_message(sock, &session->players[id], &apple_message);
    }

    server_message response = {
        .type = MSG_CLIENTS_INFO,
        .left_time = session->session_time,
        .apple_cord = session->apple_cord
    };
    apply_server_settings(&response);
    memcpy(response.players, session->players_client,
           sizeof(session->players_client));

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (session->players[i].ready) {
            send_server_message(sock, &session->players[i], &response);
        }
    }
    return 0;
}


void session_end(session_info *session, int sock, session_info *finished_session)
{
    session->session_number++;
    *finished_session = *session;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!session->players[i].ready) continue;
        server_message message = {
            .type = MSG_GAME_OVER,
            .left_time = session->session_time
        };
        apply_server_settings(&message);
         memcpy(message.players, session->players_client,
             sizeof(session->players_client));
        send_server_message(sock, &session->players[i], &message);
    }

    memset(session->players_client, 0, sizeof(session->players_client));
    memset(session->players, 0, sizeof(session->players));
    session->ready_players = 0;
    session->session_time = 0;
}
void run_game(session_info *session,
              const session_info *finished_session,
              int server_sock,
              size_t start_time)
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
            action_handle(session, finished_session, server_sock);
        }
    }
}
int main(void)
{
    if (MAX_PLAYERS > PROTOCOL_MAX_PLAYERS) {
        fprintf(stderr, "MAX_PLAYERS cannot exceed %d\n", PROTOCOL_MAX_PLAYERS);
        return 1;
    }
    if (PLAYERS_TO_START > MAX_PLAYERS) {
        fprintf(stderr, "PLAYERS_TO_START cannot exceed MAX_PLAYERS\n");
        return 1;
    }

    srand(time(NULL));
    session_info session; 
    memset(&session, 0, sizeof(session));
    session.session_number = 1;
    session_info finished_session;
    memset(&finished_session, 0, sizeof(finished_session));

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
        wait_players(&session, &finished_session, server_sock);
        printf("GAME START\n");

        time_t start_time = time(NULL);
        run_game(&session, &finished_session, server_sock, start_time);

        printf("game ended\n");
        session_end(&session, server_sock, &finished_session);
        printf("Session number: %d\nWait new player...\n", session.session_number);
    }

    close(server_sock);
    return 0;
}
