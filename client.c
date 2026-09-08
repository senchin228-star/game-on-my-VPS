#include "config.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int menu_start()
{
    int key;
    while(1){
        printf("To start, enter Y.");
        key = fgetc(stdin);
        if (key == 'Y') break;
        continue;
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
int send_move(int sock, struct sockaddr_in server_addr, PLAYER_SIGNALS sig)
{
    int signal = sig;
    int bytes_sent = sendto(sock, &signal, sizeof(int), 0,
            (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_sent <= 0){
        perror("Failed to send the signal");
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
        perror("Failed to send the request");
        return 1;
    }
    return 0;
}

PLAYER_SIGNALS get_server_sig(int sock)
{
    PLAYER_SIGNALS sig;
    int bytes_received = recvfrom(sock, &sig, sizeof(PLAYER_SIGNALS), 0, NULL, NULL);
    if (bytes_received <= 0){
        perror("Get server signal error");
        return NO_KEY;
    }
    return sig;
}

int main()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket create error");
        return 1;
    }
    
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &client_addr.sin_addr) <= 0){
        perror("Wrong IP addres");
        return 1;
    }

    menu_start();
    if (join_request(sock, client_addr) != 0) return 1;;
    PLAYER_SIGNALS sig = get_server_sig(sock);
    if (sig == PLAYER_JOIN_ACCEPT)
    {
        printf("Connect!\n");
    }else printf("Full lobby");

    return 0;
}
