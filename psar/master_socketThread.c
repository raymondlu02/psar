#include "psar.h"


// gcc -Wall -Wextra -pthread master_socketThread.c master.c -o master -lm
// gcc -Wextra -pthread slaveFonc.c test_general.c  -o slave -lm

// Ce code correspont a loop master je pense

#define PORT 32013
#define MAX_LISTEN 5

// adresse du debut de la mmap
caddr_t addr_map;

Lpage *lpage = NULL;
int sizeSharedmem;

char msg[4097];

unsigned page_shift;/* Nombre de bits d'un page */
int page_size;

// adr = numero de page
// envoie la page a l'esclave
void lock_read(void *adr, int s, int slave_socket){

    int numPage = *(int*)adr;
    Lpage *tmp = lpage; 
    caddr_t debut_page= addr_map+ s*(numPage);   

    while (tmp!= NULL && tmp->numPage != numPage){
        tmp = tmp->suivant;

    } 
        
    // trater le cas ou il doit attendre car il y a qqun qui souhaite ecrire 
    sem_wait(&tmp->semE);


    // La lecture commence ici
    sem_wait(&tmp->semL);

    tmp->nbUserRead++; 

    sem_post(&tmp->semE);

    if (tmp->PageUser[slave_socket] == 0){
        // ici la socket n'a pas la page a jour
        printf("la slave_socket %d  prend la page %d en lecture\n\n",slave_socket,numPage );
        // doit envoyer sa page
        sprintf(msg, "%d", SEND_PAGE);   
        for (int i =1; i< page_size+1; i++){
            msg[i]= debut_page[i-1];
        }
        
        send(slave_socket,msg,page_size+1,0);
        tmp->PageUser[slave_socket] = 1;
    } else{
        // ici la socket a la page a jour
        printf(" l'esclave peux lire sa propre page\n\n");
        // dis qu'il peut lire la sienne
        sprintf(msg, "%d", NO_PAGE);  
        send(slave_socket,msg,page_size+1,0);
    }
    // fin lecture
    memset(msg, '0', page_size+1);
    
}
//---------------------------------------------------------------------------

// relache la lecture d'un esclave
void unlock_read(void *adr){

    int numPage = *(int*)adr;

    Lpage *tmp = lpage;  
    while (tmp!= NULL && tmp->numPage != numPage){
        tmp = tmp->suivant;
    } 
    tmp->nbUserRead--; // nombre de processus qui lisent la page
    sem_post(&tmp->semL);
    printf("relache la lecture de la page %d\n\n",numPage);
}
// -----------------------------------------------------------------------------

// s est la taille d'une page
// adr = numero de page
// l'ecriture de la page, envoie la page a l'esclave
void lock_write(void *adr, int s, int slave_socket){



    int numPage = *(int*)adr;
    Lpage *tmp = lpage; 
    caddr_t debut_page= addr_map+ s*(numPage);

    // trouve la structure de la page
    while (tmp!= NULL && tmp->numPage != numPage){
        tmp = tmp->suivant;

    } 
         
    // prend tout les semphores lecture et ecriture
    sem_wait(&tmp->semE); 
    int cpt =0;
    while(cpt != MAX_SLAVE-1){
        sem_wait(&tmp->semL);
        cpt++;
    }

    // a partir de la, il peut ecrire     
    lpage->idWriter = slave_socket;

    
    if (tmp->PageUser[slave_socket] == 0){
        // ici la socket n'a pas la page a jour
        printf("la slave_socket %d  prend la page %d en ecriture\n\n",slave_socket,numPage );
        // doit envoyer sa page
        sprintf(msg, "%d", SEND_PAGE);   
        for (int i =1; i< page_size+1; i++){
            msg[i]= debut_page[i-1];
        }
        printf("msg = %s\n",msg);
        send(slave_socket,msg,page_size+1,0);
        tmp->PageUser[slave_socket] = 1;
    } else{
        // ici la socket a la page a jour
        printf("l'esclave peux lire sa propre page\n\n");
        // dis qu'il peut lire la sienne
        sprintf(msg, "%d", NO_PAGE);  
        send(slave_socket,msg,page_size+1,0);
    }

    memset(msg, '0', page_size+1);

}
//---------------------------------------------------------------------------
// adr la page
// libere la page receptionne la taille, l'offset et les donnees a modifier

void unlock_write(void *adr, int slave_socket){

    int numPage = (int)(*(long*)adr);
    Lpage *tmp = lpage;  
    while (tmp->numPage != numPage){
        tmp = tmp->suivant;
    } 
    tmp->idWriter=0;

    // reception de la taille de donnees a modifié
    int taille;
    recv(slave_socket,&taille,sizeof(int),0);

    // reception de l'offset de la map
    int offset;
    recv(slave_socket , &offset, sizeof(int),0); 

    // reception des donnees a modifier
    int* buffer = malloc ((sizeof(int)*(taille+1)));
    recv(slave_socket , buffer, sizeof(int)*(taille),0); 
    buffer[taille]='\0';


    // modifie a la bonne case
    int* map_tmp = (int*)(addr_map+offset); 
    for (int i = 0; i<taille; i++){
        map_tmp[i]=buffer[i];
        printf ("a l'offset = %d map[%d] = %d\n",offset,i, map_tmp[i]);
    }

    free(buffer);
    // remet tout les user qui ont la bonne version a 0 sauf sois meme
    for(int i = 0; i<50;i++ ){
        if (i!= slave_socket){
            tmp->PageUser[slave_socket] = 0;
        }
    }



    sem_post(&tmp->semE);

    int cpt =0;
    while(cpt != MAX_SLAVE-1){
        sem_post(&tmp->semL);
        cpt++;
    }
    

    printf("fin unlock_write\n");
}

//---------------------------------------------------------------------------

//thread ecoute des esclave a remplir avec la gestion des esclave
void * Slave(int *socket){
    
    int slave_socket = *socket;
    // je sais plus pk
    // numSocket = socket;

    int s = getpagesize(); 
    int buffer[TAILLE_BUF];

    while (1){

        // reception du numero de la requete + d'un numero de page 
        recv(slave_socket , buffer, sizeof(int)*TAILLE_BUF,0);

        if (buffer[0]==END){
            printf("la socket %d s'est deconnecté\n",slave_socket );
            return NULL;
        }
        
        long req = buffer[0];   // requete
        long adr = buffer[1];   // page


        printf("numero de requete =%ld et de page =%ld \n",req,adr);
        
        if (req==0){
            printf("error\n");
            return NULL;
        }

        // appelle de la bonne requete
        if (req == LOCK_READ1 ){
            //printf("passe\n");
            lock_read(&adr,s, slave_socket);
        }
        if (req == UNLOCK_READ1 ){
            unlock_read(&adr);
        }
        if (req == LOCK_WRITE1  ){
            lock_write(&adr, s, slave_socket);
        }
        if (req == UNLOCK_WRITE1 ){
            unlock_write(&adr, slave_socket);
        }
        buffer[0]=-10;
        buffer[1]=-10;
    }
    return NULL;
}


// retourne l'adresse de la memoire partagée
void *InitMaster(int size){
    // initialise taille page  
    page_size = getpagesize();
    page_shift = log2(getpagesize());

    sizeSharedmem = size;    
    // initialise map
    int flags = MAP_PRIVATE| MAP_FILE;
    int fd;
    if ((fd = open("/dev/zero", O_RDWR)) == -1) {
        perror("open");
        exit(1);
    }

    if (size % page_size )
        size = (((unsigned) size >> page_shift) << page_shift) + page_size;

    addr_map = mmap(0, size, PROT_READ|PROT_WRITE, flags, fd, 0);
    if (addr_map == MAP_FAILED)
        perror("mmap");


    
    // initialise structure 
    lpage = malloc(sizeof(Lpage));
    Lpage *tmp = lpage;
    int cpt = 0;
    while (cpt<= (size/page_size)){
        tmp->numPage = cpt;
        tmp->nbUserRead = 0;
        tmp->idWriter=0;

        //j'ai changer la taille (grande partie du tableau non utiliser)
        for (int i =0; i< 50; i++){
            tmp->PageUser[i]=0;
        }

        sem_init(&tmp->semL,0,MAX_SLAVE-1);
        sem_init(&tmp->semE,0,1);
        

        tmp->suivant = malloc(sizeof(Lpage));
        tmp = tmp->suivant;
        
        cpt++;
        
    }
    tmp->suivant=NULL;
    

    // pour tester 

    for (int i=0;i<size; i= i+1){
        addr_map[i]=0; 
    }

    for (int i=0;i<size; i= i+4){
        addr_map[i]=i; 
    }

    // fin du test

    printf("le maitre a ete initialisé\n");
     // doit retourner le retour de mmap
    return addr_map;
    
}   

//normalement vous toucher pas a ca
void LoopMaster(){


    int server_socket;
    int slave_sockets[MAX_SLAVE]; //tableau socket esclave
    
    pthread_t threads[MAX_SLAVE]; //tableau thread

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    int addrlen = sizeof(address);

    //pour eviter le probleme : bind() addr already in use



    // cree la socket du maitre
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1){
        perror("socket()");
        exit(errno);
    }

    int true =1;
    if (setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int)) == -1){
        perror("Setsockopt");
        exit(1);
    }

    //lie a la struc sockaddr_in
    if(bind (server_socket, (struct sockaddr *) &address, sizeof(address)) == -1){
        perror("bind()");
        exit(errno);
    }
    
    //
    if (listen(server_socket, MAX_LISTEN) == -1){
        perror("listen");
        exit(errno);
    }
    
    //cree les socket esclave + un thread par esclave
    int cpt = 0;
    int t = 0;
    while(1){
        if ((slave_sockets[cpt] = accept(server_socket, (struct sockaddr *)&address, (socklen_t*) &addrlen))<0){
            perror("accept");
            exit(errno);
        }
        send(slave_sockets[cpt],&sizeSharedmem,sizeof(int),0);
        printf("la slave socket = %d s'est connecté \n",slave_sockets[cpt]);
        // avant yavait (void*) slave_socket[cpt]

        t = pthread_create(&threads[cpt],NULL,(void*)* Slave, &(slave_sockets[cpt]));
        if(t){
            perror("pthread_create");
            exit(errno);
        }
        cpt++;

    }
    // faut rajouter une terminaison propre (ce fini quand max slave est atteint actuellement)
    for(int i=0;i<cpt;i++){
        pthread_join(threads[i],NULL);
        close(slave_sockets[i]); //je sais pas si y'a besoin
    }
    close(server_socket);
    printf("fin du maitre\n");
}

