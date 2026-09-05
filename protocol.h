#define MAX_PALYERS 2
#define PLAYERS_TO_START 2
#define TIME_FOR_EXIT 60
typedef struct {
    struct sockaddr_in player_addr;
    int cord[2];
    int ready;
} PlayerInfo;

typedef struct {
    int session_number;
    PlayerInfo players[MAX_PLAYERS];
    int ReadyPlayers;
    int SessionTime;
} session_info;

typedef enum {
    PLAYER_JOIN,
    UP_KEY,
    DOWN_KEY,
    LEFT_KEY,
    RIGHT_KEY
} PLAYER_SIGNALS;

