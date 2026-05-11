#include <string.h>
#include <stdio.h>
#include "../include/ttt.h"
#include <stdlib.h>
#include <unistd.h>

//sockets
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

const bool DEBUG=false;

typedef struct Client {
    int clientSocket;
    int win;
    int loss;
    int draw;
} Client;

Client* client_new(){
    Client* c=malloc(sizeof(Client));
    c->clientSocket=-1;
    c->win=0;
    c->loss=0;
    c->draw=0;
    return c;
}

void client_destroy(Client* c){
    if(c->clientSocket>0) close(c->clientSocket);
    free(c);
}

void client_connectToServer(Client* c, char* ip, int port){ //needs error checking eventually
    c->clientSocket=socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);

    if(inet_pton(AF_INET, ip, &addr.sin_addr)<=0){
        struct hostent* server=gethostbyname(ip);
        if(server==NULL){
            printf("Invalid host\n");
            exit(1);
        }
        addr.sin_addr=*(struct in_addr*)server->h_addr_list[0];
    }

    if(connect(c->clientSocket, (struct sockaddr*)&addr, sizeof(addr))<0){
        printf("Failed to connect\n");
        exit(1);
    }
}

void client_sendCommand(Client* c, char* cmd){
    if(c->clientSocket<0){
        printf("Not connected\n");
        return;
    }

    char fullCmd[32];
    snprintf(fullCmd, sizeof(fullCmd), "%s\n", cmd);

    if(send(c->clientSocket, fullCmd, strlen(fullCmd), 0)<=0){
        printf("Disconnected\n");
        exit(0);
    }
    if(DEBUG)printf("Sent %s", fullCmd);
}

void print_board(char cells[]){
    printf("-------------\n");
    for (int row=0; row<3; row++) {
        printf("|");

        for (int col=0; col<3; col++) {
            int i=row*3+col;

            switch (cells[i]) {
                case ' ': printf(" %d |", i+1); break;
                default: printf(" %c |", cells[i]); break;
            }
        }
        printf("\n-------------\n");
    }
}

char* client_receiveServerResponse(Client* c) { //reads stream one by one to account for multiple messages in quick succession
    if (c->clientSocket<0) {
        printf("Not connected\n");
        return NULL;
    }

    char bytes[128];
    ssize_t bytesLen=0;

    while (1) {
        if(recv(c->clientSocket, bytes+bytesLen, 1, 0)<=0) return NULL;
        
        bytesLen++;
        
        if (bytes[bytesLen-1]=='\n') {
            bytes[bytesLen-1]='\0';

            char* response=malloc(bytesLen);
            strcpy(response, bytes);
            return response;
        }
    }
}

void client_handleServerResponse(Client* c){
    while(1){
        char* response=client_receiveServerResponse(c);
        if(response==NULL){
            printf("Timed out, disconnected\n");
            exit(0);
        }

        char* operation=strtok(response, ";");
        char* data=strtok(NULL, "");

        if (strcmp(operation, "ERR")==0){
            printf("Error: %s\n> ", data);
            break;
        }

        if (strcmp(operation, "READY")==0){
            printf("Play another round? (JOIN/QUIT)\n> ");
            break;
        }

        if (strcmp(operation, "ACT")==0){
            printf("> ");
            break;
        }

        if (strcmp(operation, "END")==0){
            printf("%s\n", data);
            continue;
        }

        if (strcmp(operation, "BOARD")==0) {
            char cells[9];

            cells[0]=(char)*strtok(data,",");
            for(int i=1;i<9;i++)
                cells[i]=(char)*strtok(NULL,",");

            print_board(cells);
            continue;
        }

        if (strcmp(operation, "JOINED")==0) {
            char* room=strtok(data, ",");
            char* player=strtok(NULL, ",");

            printf("Joined room %s", room);
            if (strcmp(player, "1")==0)
                printf(". Waiting for player 2.\n");
            else
                printf(".\n");

            continue;
        }

        if (strcmp(operation, "START")==0){
            if (data[0]=='1'){
                printf("Game started. You will move first.\n> ");
                break;
            }else{
                printf("Game started. Waiting for Player 1 to move.\n");
                continue;
            }
        }

        if (strcmp(operation, "STAT")==0) {
            char* win=strtok(data, ",");
            char* loss=strtok(NULL, ",");
            char* draw=strtok(NULL, ",");

            printf("Results:\nGames WON: %s\nGames LOST: %s\nGames DRAWN: %s\n",win,loss,draw);
            exit(0);
        }

        printf("Bad response from server: %s\n", response);
        exit(0);
    }
}

// Standard input parser for your command line program
void input_parser(Client* c) {
    char command[16];

    if (fgets(command, sizeof(command), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }

    command[strcspn(command, "\n")] = '\0';

    char cmd[16];
    int n;
    char extra[1000];

    if (sscanf(command, "%16s %d %99s", cmd, &n, extra) < 1) {
        printf("Invalid command\n");
        return;
    }

    //the plan is to send all commands to the server, regardless of validity
    char fullCmd[32];
    if(n==-1)
        snprintf(fullCmd, sizeof(fullCmd), "%s;", cmd);
    else
        snprintf(fullCmd, sizeof(fullCmd), "%s;%d", cmd, n);

    client_sendCommand(c, fullCmd);
    client_handleServerResponse(c);
}

int main(int argc, char* argv[]) {
    if (argc!=3) {
        printf("Usage: ./client <server_ip> <port>\n");
        return 1;
    }

    Client* c=client_new();
    client_connectToServer(c, argv[1], atoi(argv[2]));

    char* response=client_receiveServerResponse(c);
    if (strcmp(response,"READY;")==0)
        printf("> ");

    while(1) input_parser(c);

    return 0;
}
