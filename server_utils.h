#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include "protocol.h"

int send_server_message(int sock, const player_info *player,
                               const server_message *message);
int same_player(const struct sockaddr_in *left,
                       const struct sockaddr_in *right);
double monotonic_seconds(void);
void print_player_pos(session_info  *session, int id);
void print_player(session_info *session, int id);
void print_players(session_info *session);

#endif

