#include "config.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>

void set_raw_mode(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

int menu_start()
{
    int key;
    while(1){
        printf("To start, enter Y.\n");
        key = fgetc(stdin);
        int c;
        while ((c = fgetc(stdin)) != '\n' && c != EOF){}
        if (key == 'Y') break;
    }
    printf("connecting..\n");
    return 0;
}
PLAYER_SIGNALS get_move()
{
    int key = fgetc(stdin);
    switch (key){
        case 'w':
            return UP_KEY;
        case 's':
            return DOWN_KEY;
        case 'd':
            return RIGHT_KEY;
        case 'a':
            return LEFT_KEY;
        default:
            return NO_KEY;
    }
}
int send_move(int sock, struct sockaddr_in *server_addr, PLAYER_SIGNALS sig)
{
    int signal = sig;
    int bytes_sent = sendto(sock, &signal, sizeof(int), 0,
            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (bytes_sent <= 0){
        perror("Failed to send the signal\n");
        return 1;
    }
    return 0;
}
int join_request(int sock, struct sockaddr_in server_addr)
{
    PLAYER_SIGNALS request = PLAYER_JOIN_REQUEST;
    int bytes_sent = sendto(sock, &request, sizeof(int), 0,
            (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_sent <= 0){
        perror("Failed to send the request\n");
        return 1;
    }
    return 0;
}

int get_server_resp(int sock)
{
    join_response resp;
    int bytes_received = recvfrom(sock, &resp, sizeof(join_response), 0, NULL, NULL);
    if (bytes_received <= 0){
        perror("Get server signal error\n");
        return -1;
    }
    return resp.id;
}

int main()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket create error\n");
        return 1;
    }
    
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &client_addr.sin_addr) <= 0){
        perror("Wrong IP addres\n");
        return 1;
    }

    while(1){
        menu_start();
        if (join_request(sock, client_addr) != 0) return 1;
        int id = get_server_resp(sock);
        if (id >= 0)
        {
            printf("Connect!\n");
            break;
        }else printf("Full lobby\n");
    }
    player_cord all_cord[MAX_PLAYERS];

    fd_set readfd;
    int retval;
    int nfds = sock + 1;
    set_raw_mode(1);
    while(1)
    {
        FD_ZERO(&readfd);
        FD_SET(STDIN_FILENO, &readfd);
        FD_SET(sock, &readfd);
        retval = select(nfds, &readfd ,NULL, NULL, NULL);
        if (retval == -1) perror("Select() error");
        else if (FD_ISSET(STDIN_FILENO, &readfd)){
           PLAYER_SIGNALS move = get_move();
           send_move(sock, &client_addr, move);
        }
        if (FD_ISSET(sock, &readfd)){
            printf("Get new pos\n");
            int bytes_received = recvfrom(sock, &all_cord, sizeof(all_cord), 0, NULL, NULL);
            if (bytes_received <= 0) perror("Failed get cord\n");
        }
    }
    set_raw_mode(0);
    return 0;
}
