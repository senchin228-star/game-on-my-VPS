#include "config.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int menu_start()
{
    printf("To start, enter Y.");
    int key;
    while(1){
        key = fgetc(stdin);
        if (key == 'Y') break;
        continue;
    }
    printf("connecting..\n");
    return 0;
}
int join_request(int sock, struct sockaddr_in server_addr)
{
    int request = PLAYER_JOIN;
    int bytes_sent = sendto(sock, &request, sizeof(int), 0,
            (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_sent <= 0){
        perror("Failed to send the request");
        return 1;
    }else{
        return 0;
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
