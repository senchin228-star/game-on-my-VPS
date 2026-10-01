#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <arpa/inet.h>
#include "config.h"

/* These structs are sent as raw UDP payloads, so client and server must use
 * the same definitions, enum values, compiler ABI, and build configuration. */
typedef enum {
    PLAYER_JOIN_REQUEST,
    PLAYER_RECONNECT_REQUEST,
    PLAYER_LEAVE,
    NO_KEY,
    UP_KEY,
    DOWN_KEY,
    LEFT_KEY,
    RIGHT_KEY,
    GET_POINT,
    SEND_CORD
} PLAYER_SIGNALS;

typedef enum {
    PLAYER_JOIN_DENIED,
    PLAYER_JOIN_ACCEPT,
    MSG_CLIENTS_INFO,
    MSG_GAME_OVER,
    MSG_GAME_START,
    MSG_LOBBY_INFO,
    MSG_GET_APPLE,
    MSG_RECONNECT_ACCEPT,
    MSG_RECONNECT_DENIED
} MESSAGE_TYPE;

typedef struct {
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t A;
} rgba_color;

typedef struct {
    int x;
    int y;
} player_cord;

typedef struct {
    player_cord cord;
    rgba_color color;
    int id;
    int ingame;
    int score;
    char nickname[128];
} player_client_info;


typedef struct {
    MESSAGE_TYPE type;
    player_client_info players[MAX_PLAYERS];
    player_cord apple_cord;
    int players_in_lobby;
    int left_time;
    int id;
    uint64_t reconnect_token;
} server_message;


typedef struct {
    PLAYER_SIGNALS type;
    player_cord cord;
    uint64_t reconnect_token;
    char nickname[128];
} player_message;

typedef struct {
    struct sockaddr_in player_addr;
    int ready;
    uint64_t reconnect_token;
} player_info;

typedef struct {
    int session_number;
    player_cord apple_cord;
    player_info players[MAX_PLAYERS];
    player_client_info players_client[MAX_PLAYERS];
    int ready_players;
    int session_time;
} session_info;

#endif



