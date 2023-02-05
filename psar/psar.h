#ifndef PSAR_H

#define PSAR_H

#define _GNU_SOURCE
#include <semaphore.h>
#include <unistd.h>



#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// master_socket
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>    
#include <math.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <signal.h>
#include <ucontext.h>
#include <features.h>
#include <execinfo.h>
#include <sys/wait.h>
#include <sys/ucontext.h>

#define TAILLE_BUF 2

// esclave
#define LOCK_READ1 1
#define LOCK_WRITE1 2
#define UNLOCK_READ1 3
#define UNLOCK_WRITE1 4
#define END 9

//maitre
#define SEND_PAGE 8
#define NO_PAGE 7


// max de machine esclave pouvant se connecter
#define MAX_SLAVE 4



void *InitMaster(int size);

void LoopMaster();

void *InitSlave(char *HostMaster,int *sock);

void req_lock_read(void *adr, int sock);

void req_unlock_read(void *adr, int sock);

void req_lock_write(void *adr, int sock);

void req_unlock_write(void *adr, int s, int sock);

void end_sock(int sock); // terminaison propre


void lock_read(void *adr, int s, int slave_socket);

void unlock_read(void *adr);

void lock_write(void *adr, int s, int slave_socket);

void unlock_write(void *adr, int slave_socket);


typedef struct Lpage Lpage;

struct Lpage{

    int numPage; // numéro page
    int nbUserRead; // nombre de processus qui lisent la page
    int idWriter; // null si aucun écrivain

    //(90% du tableau vide)
    int PageUser[50]; // liste des esclaves qui ont déjà la bonne version de la page

    // utiliser 1 semaphore pour lire initialiser a MAX_SLAVE
    // 1 semaphore pour ecrire initialisé a 0
    sem_t semL;
    sem_t semE;
    Lpage *suivant;
};




#endif