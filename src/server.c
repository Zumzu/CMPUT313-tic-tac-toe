#include <string.h>
#include "../include/ttt.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

//sockets
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

typedef struct PlayerConnection {
    int socket;
    int win;
    int loss;
    int draw;
    bool disconnect;
} PlayerConnection;

typedef struct GameRoom {
    PlayerConnection* player1;
    PlayerConnection* player2;
    TicTacToe* t; //null denotes no active game
    // This lock need to be acquired before accessing shared variables (e.g.,
    // player1Socket, player2Socket, numActivePlayers), and unlocked afterward
    int acting; //either 1 or 2
    int numActivePlayers;
    pthread_mutex_t roomMutex;
    pthread_t timer;
} GameRoom;

typedef struct Server {
    int serverSocket;
    GameRoom* rooms[5];
} Server;

typedef struct ThreadArg {
    Server* server;
    int clientSocket;
} ThreadArg;

typedef struct TimerArg {
    Server* server;
    PlayerConnection* p;
} TimerArg;

void* server_timeout(void* arg);

Server* server_new(){
    Server* s=malloc(sizeof(Server));
    s->serverSocket=-1;
    for(int i=0;i<5;i++){
        s->rooms[i]=malloc(sizeof(GameRoom));
        s->rooms[i]->player1=NULL;
        s->rooms[i]->player2=NULL;
        s->rooms[i]->t=NULL;
        s->rooms[i]->acting=1;
        s->rooms[i]->numActivePlayers=0;
        pthread_mutex_init(&(s->rooms[i]->roomMutex), NULL);
        s->rooms[i]->timer=0;
    }
    return s;
}

void server_destroy(Server* s){
    if(s->serverSocket>0) close(s->serverSocket);

    for(int i=0;i<5;i++){
        if(s->rooms[i]->t) ttt_destroy(s->rooms[i]->t);
        pthread_mutex_destroy(&(s->rooms[i]->roomMutex));

        if(s->rooms[i]->player1) free(s->rooms[i]->player1);
        if(s->rooms[i]->player2) free(s->rooms[i]->player2);

        free(s->rooms[i]);
    }

    free(s);
}

void server_respond(int socket, char* cmd){
    if(socket<0){
        printf("Not connected");
        return;
    }

    char fullCmd[32];
    snprintf(fullCmd, sizeof(fullCmd), "%s\n", cmd);

    send(socket, fullCmd, strlen(fullCmd), 0);
    printf("Respond %s", fullCmd);
}

char* server_receiveClientResponse(Server* s, PlayerConnection* p) {
    if (p->socket<0) {
        printf("Not connected\n");
        return NULL;
    }

    char bytes[128];
    ssize_t bytesLen=0;

    while (1) {
        if(recv(p->socket, bytes+bytesLen, 1, 0)<=0 || p->disconnect) return NULL;

        bytesLen++;

        if (bytes[bytesLen-1]=='\n') {
            bytes[bytesLen-1]='\0';

            char* response=malloc(bytesLen);
            strcpy(response, bytes);
            return response;

        }else if (bytesLen >= sizeof(bytes)) {
            printf("Message too long\n");
            return NULL;
        }
    }
}

void server_sendBoard(int socket, TicTacToe* t){
    char board[32];
    snprintf(board, sizeof(board), "BOARD;%c,%c,%c,%c,%c,%c,%c,%c,%c",t->board[0][0],t->board[0][1],t->board[0][2],t->board[1][0],t->board[1][1],t->board[1][2],t->board[2][0],t->board[2][1],t->board[2][2]);

    server_respond(socket, board);
}

void server_disolveRoom(GameRoom* room){ //REQUIRES LOCK BEFORE CALL
    ttt_destroy(room->t);
    room->t=NULL;
    room->player1=NULL;
    room->player2=NULL;
    room->acting=1;
    room->numActivePlayers=0;
}

void server_handleMark(Server* s, PlayerConnection* p, char* data){
    GameRoom* room=NULL;
    PlayerConnection* opponent=NULL;

    for(int i=0;i<5;i++){
        if(s->rooms[i]->player1==p){
            room=s->rooms[i];
            opponent=room->player2;
            break;
        }else if(s->rooms[i]->player2==p){
            room=s->rooms[i];
            opponent=room->player1;
            break;
        }
    }

    if(!room || !opponent){
        server_respond(p->socket, "ERR;Not in active game");
        return;
    }

    if((room->player1==p && room->acting==2) || (room->player2==p && room->acting==1)){
        server_respond(p->socket, "ERR;Not your turn");
        return;
    }

    if(!data){
        server_respond(p->socket, "ERR;Invalid cell number");
        return;
    }

    if(!(strcmp(data,"1")==0||strcmp(data,"2")==0||strcmp(data,"3")==0||strcmp(data,"4")==0||strcmp(data,"5")==0||strcmp(data,"6")==0||strcmp(data,"7")==0||strcmp(data,"8")==0||strcmp(data,"9")==0)){
        server_respond(p->socket, "ERR;Invalid cell number");
        return;
    }

    pthread_mutex_lock(&room->roomMutex); //start of non-reads

    if(!ttt_makeMove(room->t, atoi(data))){
        pthread_mutex_unlock(&room->roomMutex);
        server_respond(p->socket, "ERR;Occupied, move again");
        return;
    }

    if(ttt_isDraw(room->t)){
        room->player1->draw++;
        room->player2->draw++;
        server_respond(room->player1->socket, "END;Its a draw");
        server_respond(room->player2->socket, "END;Its a draw");
        server_sendBoard(room->player1->socket, room->t);
        server_sendBoard(room->player2->socket, room->t);

        server_respond(room->player1->socket, "READY;");
        server_respond(room->player2->socket, "READY;");

        if(room->timer!=0) {
            pthread_cancel(room->timer);
            pthread_join(room->timer, NULL);
            room->timer = 0;
        }
        server_disolveRoom(room);


    }else if(ttt_hasWinner(room->t)){
        if (ttt_checkWin(room->t,'X')){
            room->player1->win++;
            room->player2->loss++;
            server_respond(room->player1->socket, "END;You won");
            server_respond(room->player2->socket, "END;You lost");

        }else{ //O wins
            room->player1->loss++;
            room->player2->win++;
            server_respond(room->player1->socket, "END;You lost");
            server_respond(room->player2->socket, "END;You won");
        }

        server_sendBoard(room->player1->socket, room->t);
        server_sendBoard(room->player2->socket, room->t);

        server_respond(room->player1->socket, "READY;");
        server_respond(room->player2->socket, "READY;");

        if(room->timer!=0) {
            pthread_cancel(room->timer);
            pthread_join(room->timer, NULL);
            room->timer = 0;
        }
        server_disolveRoom(room);

    }else{ //game continues
        ttt_switchPlayer(room->t);
        room->acting=room->acting==1?2:1;
        server_sendBoard(opponent->socket, room->t);
        server_respond(opponent->socket, "ACT;");

        if(room->timer!=0) {
            pthread_cancel(room->timer); //set timer for other player
            pthread_join(room->timer, NULL);
            room->timer = 0;
        }

        TimerArg* timer_arg=malloc(sizeof(TimerArg));
        timer_arg->server=s;
        timer_arg->p=room->acting==1?room->player1:room->player2;
        pthread_create(&room->timer, NULL, server_timeout, timer_arg);
    }

    pthread_mutex_unlock(&room->roomMutex);
}

void server_handleJoin(Server* s, PlayerConnection* p, char* data){
    for(int i=0;i<5;i++){
        if(s->rooms[i]->player1==p || s->rooms[i]->player2==p){
            server_respond(p->socket, "ERR;Already in a room");
            return;
        }
    }

    if(!data){
        server_respond(p->socket, "ERR;Invalid room");
        return;
    }

    if(!(strcmp(data,"1")==0||strcmp(data,"2")==0||strcmp(data,"3")==0||strcmp(data,"4")==0||strcmp(data,"5")==0)){
        server_respond(p->socket, "ERR;Invalid room");
        return;
    }

    GameRoom* room=s->rooms[atoi(data)-1];
    pthread_mutex_lock(&room->roomMutex);

    if(room->numActivePlayers==2){
        server_respond(p->socket, "ERR;Room full");
        pthread_mutex_unlock(&room->roomMutex);
        return;
    }

    if(room->numActivePlayers==0){
        room->player1=p;
        room->numActivePlayers=1;
        char msg[16];
        snprintf(msg,sizeof(msg), "JOINED;%s,1", data);
        server_respond(p->socket, msg);

    }else{
        room->player2=p;
        room->numActivePlayers=2;
        char msg[16];
        snprintf(msg,sizeof(msg), "JOINED;%s,2", data);
        server_respond(p->socket, msg);

        room->t=ttt_new();

        server_respond(room->player1->socket, "START;1");
        server_respond(room->player2->socket, "START;2");

        TimerArg* timer_arg=malloc(sizeof(TimerArg));
        timer_arg->server=s;
        timer_arg->p=room->player1;
        pthread_create(&room->timer, NULL, server_timeout, timer_arg);
    }

    pthread_mutex_unlock(&room->roomMutex);
}

void server_handleDisconnect(Server* s, PlayerConnection* p){
    for(int i=0;i<5;i++){
        pthread_mutex_lock(&s->rooms[i]->roomMutex);
        if(s->rooms[i]->player1==p){
            s->rooms[i]->player1=NULL;
            s->rooms[i]->numActivePlayers--;

            if(s->rooms[i]->t){
                server_respond(s->rooms[i]->player2->socket, "END;You win by forfeit");
                s->rooms[i]->player2->win++;
                server_respond(s->rooms[i]->player2->socket, "READY;");
                server_disolveRoom(s->rooms[i]);
            }

        }else if(s->rooms[i]->player2==p){ //its extremely rare that there wouldnt be a TTT running in this case
            s->rooms[i]->player2=NULL;
            s->rooms[i]->numActivePlayers--;

            if(s->rooms[i]->t){
                server_respond(s->rooms[i]->player1->socket, "END;You win by forfeit");
                s->rooms[i]->player1->win++;
                server_respond(s->rooms[i]->player1->socket, "READY;");
                server_disolveRoom(s->rooms[i]);
            }
        }
        pthread_mutex_unlock(&s->rooms[i]->roomMutex);
    }
}

void* server_timeout(void* arg){
    printf("Timeout start\n");
    TimerArg* args=(TimerArg*)arg;
    Server* s=args->server;
    PlayerConnection* p=args->p;

    sleep(15);

    printf("Time out\n");
    p->disconnect=true;
    server_handleDisconnect(s,p);
    free(args);
    return NULL;
}

bool server_handleQuit(Server* s, PlayerConnection* p){
    for(int i=0;i<5;i++){
        if(s->rooms[i]->player1==p || s->rooms[i]->player2==p){
            server_respond(p->socket, "ERR;Not allowed");
            return false;
        }
    }

    char msg[16];
    snprintf(msg, sizeof(msg), "STAT;%d,%d,%d",p->win,p->loss,p->draw);
    server_respond(p->socket, msg);
    return true;
}

void* server_handleNewClient(void* arg){
    ThreadArg* args=(ThreadArg*)arg;
    Server* s=args->server;

    PlayerConnection* p=malloc(sizeof(PlayerConnection));
    p->socket=args->clientSocket;
    p->win=0;
    p->loss=0;
    p->draw=0;
    p->disconnect=false;

    free(args);

    printf("Thread started\n");
    server_respond(p->socket, "READY;");

    char* message;
    while(1) {
        message=server_receiveClientResponse(s,p);

        printf("Got %s\n", message);
        if(message==NULL) break;

        char* operation=strtok(message, ";");
        char* data=strtok(NULL, "");

        if(strcmp(operation, "JOIN")==0){
            server_handleJoin(s,p,data);

        }else if (strcmp(operation, "MARK")==0){
            server_handleMark(s,p,data);

        }else if (strcmp(operation, "QUIT")==0){
            if(server_handleQuit(s,p)) break;
        }else{
            server_respond(p->socket, "ERR;Invalid operation");
        }
        free(message);
    }

    free(message);

    if(!p->disconnect){ //if triggered by ctrl-c, needs to clean up timer for that room, which cannot happen in within handleDisconnect, because then the timer would kill itself before unlocking the room mutex
        for(int i=0;i<5;i++){
            if((s->rooms[i]->player1==p || s->rooms[i]->player2==p) && s->rooms[i]->timer!=0){
                pthread_cancel(s->rooms[i]->timer);
                pthread_join(s->rooms[i]->timer, NULL);
                s->rooms[i]->timer = 0;
            }
        }
        server_handleDisconnect(s,p);
    }
    close(p->socket);
    free(p);

    printf("Thread killed\n");
    return NULL;
}

void server_start(Server* s, int port){
    s->serverSocket=socket(AF_INET, SOCK_STREAM, 0);

    int opt=1;
    setsockopt(s->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //set reuse address for bind

    //https://man7.org/linux/man-pages/man2/bind.2.html
    //bind expects weird casting for versitility
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(port);

    if(bind(s->serverSocket, (struct sockaddr*)&addr, sizeof(addr))<0){
        printf("Bind failed\n");
        exit(1);
    }

    if(listen(s->serverSocket, 20)<0){
        printf("Listen failed\n"); //20 max clients
        exit(1);
    }

    printf("Started\n");

    while(1){
        struct sockaddr_in clientAddr;
        socklen_t clientLen=sizeof(clientAddr);
        int clientSocket=accept(s->serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        ThreadArg* arg = malloc(sizeof(ThreadArg));
        arg->server=s;
        arg->clientSocket=clientSocket;

        pthread_t threadID;
        pthread_create(&threadID, NULL, server_handleNewClient, arg);
        pthread_detach(threadID);
    }
}

int main(int argc, char* argv[]) {
    if (argc!=2) {
        printf("Usage: ./server <port>\n");
        return 1;
    }

    int port=atoi(argv[1]);
    if (!(1024<=port && port<=65535)) {
        printf("Port must be in range [1024,65535]");
        return 1;
    }

    Server* s=server_new();
    server_start(s, port);
    return 0;
}
