#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_PLAYERS 2
#define PLAYERS_TO_START 2
#define TIME_FOR_EXIT 60
#define COUNTDOWN_SECONDS 5
#include <arpa/inet.h>
#include <SDL2/SDL.h>

typedef enum {
    PLAYER_JOIN_REQUEST,
    NO_KEY,
    UP_KEY,
    DOWN_KEY,
    LEFT_KEY,
    RIGHT_KEY,
    SEND_CORD
} PLAYER_SIGNALS;

typedef enum {
    PLAYER_JOIN_DENIED,
    PLAYER_JOIN_ACCEPT,
    MSG_CLIENTS_INFO,
    MSG_GAME_OVER,
    MSG_GAME_START,
    MSG_LOBBY_INFO
} MESSAGE_TYPE;

typedef struct {
    Uint8 R;
    Uint8 G;
    Uint8 B;
    Uint8 A;
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
    char nickname[128];
} player_client_info;


typedef struct {
    MESSAGE_TYPE type;
    player_client_info players[MAX_PLAYERS];
    int players_in_lobby;
    int left_time;
    int id;
} server_message;


typedef struct {
    PLAYER_SIGNALS type;
    player_cord cord;
    char nickname[128];
} player_message;

typedef struct {
    struct sockaddr_in player_addr;
    int ready;
} player_info;

typedef struct {
    int session_number;
    player_info players[MAX_PLAYERS];
    player_client_info players_client[MAX_PLAYERS];
    int ready_players;
    int session_time;
} session_info;

#endif



