#define MAX_PLAYERS 2
#define PLAYERS_TO_START 2
#define TIME_FOR_EXIT 60
#include <arpa/inet.h>
typedef enum {
    PLAYER_JOIN_ACCEPT,
    PLAYER_JOIN_DENIED,
    PLAYER_JOIN_REQUEST,
    NO_KEY,
    UP_KEY,
    DOWN_KEY,
    LEFT_KEY,
    RIGHT_KEY
} PLAYER_SIGNALS;

typedef struct {
    int id;
    int x;
    int y;
} player_cord;

typedef struct {
    PLAYER_SIGNALS status;
    int id;
} join_response;

typedef struct {
    struct sockaddr_in player_addr;
    int ready;
} player_info;

typedef struct {
    int session_number;
    player_info players[MAX_PLAYERS];
    player_cord cord[MAX_PLAYERS];
    int ready_players;
    int session_time;
} session_info;



