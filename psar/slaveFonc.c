
#define PORT 32013



#include "psar.h" 

int page_size ;
int page_shift ;

caddr_t addr_map;
int sock;

int createSocket(char *HostMaster){
    int sock = 0;
    struct sockaddr_in serv_addr;

    //socket esclave
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket()");
        exit(errno);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, HostMaster, &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        printf("\nConnection Failed \n");
        return -1;
    }

    return sock;

}


static void tsigsegv(int sig, siginfo_t *si, void *context)
{


    printf("signal %d\n",sig);
    /* adresse de la page qui a fait le defaut alignée  */    
    long addr = ((long) si->si_addr >> page_shift) << page_shift ; 

    /* type de l'erreur */
    int err = ((ucontext_t*)context)->uc_mcontext.gregs[REG_ERR];

    printf("Fault on page Ox%lx !\n", addr >> page_shift);
    printf("Access ");
    if (err & 0x2) {
        printf("WRITE \n");
        req_lock_write(&addr, sock);
        

    }else{ 
        printf("READ \n");
        req_lock_read(&addr, sock);
        
    }
    
}

// revoie une memoire partagée 
void *InitSlave(char *HostMaster,int *sock1){
    //pas sur qu'il faut initialiser la socket ici en regardant le test du prof

    sock = createSocket(HostMaster);
    *sock1 = sock;
    // initialise taille page  
    page_size = getpagesize();
    page_shift = log2(getpagesize());

    //int 
    int size;
    recv(sock,&size,sizeof(int),0);

    
    // initialise map
    int flags = MAP_PRIVATE| MAP_FILE;
    int fd;
    if ((fd = open("/dev/zero", O_RDWR)) == -1) {
        perror("open");
        exit(1);
    }
    // arrondir le nombre de page au superieur
    if (size % page_size )
        size = (((unsigned) size >> page_shift) << page_shift) + page_size;
    
    // au debut aucun droit de lecture ni ecriture sur la memoire
    addr_map = mmap(0, size, 0, flags, fd, 0);
    if (addr_map == MAP_FAILED)
        perror("mmap");


    // definie le handler du defaut de page ( handler de SIGSEGV )
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = tsigsegv; 
    if (sigaction(SIGSEGV, &sa, NULL) == -1){  
      perror("sigaction");
      exit(1);
    }
    return addr_map;
    
}

int *msg_requete(int *msg,int numreq,int numpage){
    msg[0] =numreq;
    msg[1]=numpage;
    return msg;
}


// adr une adresse de debut de page
// sock la socket 
// recupere la page du maitre pour la lire
void req_lock_read(void *adr, int sock){

    // numero de la page
    int numpage =(int) (((*(long *)adr)-(long)addr_map)/page_size);
    printf("numero de page req lock read = %d \n",numpage);
    // debut de l'adresse a modifier
    caddr_t debut_page = (caddr_t)*(long*)adr;

    int msg[2];
    msg_requete(msg,LOCK_READ1,numpage);

    send(sock,(char*)msg,sizeof(int)*TAILLE_BUF,0);

    //type de la reponse + page (authorisation OU authorisation + page)
    char rep[page_size+1];
    recv(sock,rep,page_size+1,0);
 

    if ( mprotect((caddr_t)(((*(long*)adr)>> page_shift)<<page_shift), page_size, PROT_READ|PROT_WRITE ) <0 ) {
        perror("mprotect");
        exit(1);
    }

    char tmp = rep[1];
    rep[1]='\0';

    if (atoi(&rep[0])==SEND_PAGE){
        // remplace sa page par celle recu
        rep[1] = tmp;
        for (int i =1; i< page_size+1; i++){
            debut_page[i-1] = rep[i];
        }

    }
    if (atoi(&rep[0])== NO_PAGE){
        // en vrai on veut qu'il fasse rien ici
        rep[1] = tmp;


    }

    // donne les droits en lecture
    if ( mprotect((caddr_t)(((*(long*)adr)>> page_shift)<<page_shift), page_size, PROT_READ ) <0 ) {
        perror("mprotect");
        exit(1);
    }
    
}

// envoie un message au maitre pour lui indiqué qu'il relache la page
void req_unlock_read(void *adr, int sock){
    long numpage = ((*(long*)adr)-(long)addr_map)/page_size;

    int msg[2];
    msg_requete(msg,UNLOCK_READ1,numpage);
    send(sock,msg,sizeof(int)*TAILLE_BUF,0);


    printf("read de la page %ld relaché\n",numpage);
    // je retire mes droits
    if ( mprotect((caddr_t)(((*(long*)adr)>> page_shift)<<page_shift), page_size, 0 ) <0 ) {
        perror("mprotect");
        exit(1);
    }

}

// adr une adresse de debut de page
// sock la socket
// recupere la page du maitre pour ecrire
void req_lock_write(void *adr, int sock){
    
    // numero de page
    long numpage = (int) (((*(long *)adr)-(long)addr_map)/page_size);
    printf("numero de page req lock write = %ld \n",numpage);
    caddr_t debut_page = (caddr_t)*(long*)adr;
    //(on peu changer) msg type de la requete
    // int msg[TAILLE_BUF];
    // sprintf(msg, "%d", LOCK_WRITE1);
    // sprintf(msg+1, "%d",numpage);
    // msg[1] = (char)numpage;

    int msg[2];
    msg_requete(msg,LOCK_WRITE1,numpage);

    send(sock,msg,sizeof(int)*TAILLE_BUF,0);

    //type de la reponse
    char rep[page_size+1];
    // int size =page_size+1;

    recv(sock,rep,page_size+1,0);
    //traitement de la reponse
    //authorisation/authorisation + page/ rien(refu authorisation) 
    if ( mprotect((caddr_t)(((*(long*)adr)>> page_shift)<<page_shift), page_size, PROT_READ|PROT_WRITE ) <0 ) {
        perror("mprotect");
        exit(1);
    }

    char tmp = rep[1];
    rep[1]='\0';

    if (atoi(&rep[0])==SEND_PAGE){
        // recois une page et donc modifie la sienne
        rep[1] = tmp;
        for (int i =1; i< page_size+1; i++){
            debut_page[i-1] = rep[i];
        }

    }
    if (atoi(&rep[0])== NO_PAGE){
        // On veut qu'il fasse rien
        rep[1] = tmp;

    }

    // les droits en lecture/ecriture ont ete données.

}


// s est la taille
// adr une adresse de debut de page
// sock est la socket
// notifie le maitre la fin d'ecrire et envoie la/les données modifié.  
void req_unlock_write(void *adr, int s, int sock){

    // numero de page
    int numpage = (int) ((*(long*)adr)-(long)addr_map)/page_size;
    
    // le type de requete a envoyer
    int msg[2];
    msg_requete(msg,UNLOCK_WRITE1,numpage);
    send(sock,msg,sizeof(int)*TAILLE_BUF,0);

    // envoie de la taille (le nombre) de donnees modifiés
    int taille;
    taille=s;
    send(sock,&taille,sizeof(int),0);

    // tableau contenant les valeurs a envoyer au maitre
    int *valeur= malloc (sizeof(int)*(s+1));
    // debut de la memoire qui a ete modifier chez le'esclave 
    int *debut= (int*)(*(long*)adr);

    // envoie de l'offset 
    int offset =(int)((*(long*)adr)-(long)addr_map);
    printf ("\noffset = %d\n", offset);
    send(sock,&offset,sizeof(int),0);

    // envoie les valeurs a modifier au maitre
    for (int i= 0; i< taille; i++){
        valeur[i] = debut[i];
        printf("valeur = %d\n",valeur[i]);
    }
    send(sock,valeur,sizeof(int)*taille,0);

    // retire les droits en lecture et ecriture
    if ( mprotect((caddr_t)(((*(long*)adr)>> page_shift)<<page_shift), page_size, 0 ) <0 ) {
        perror("mprotect");
        exit(1);
    }
    
}


// fonction a appeler pour mettre fin a la connexion
void end_sock(int sock){
    int msg =END;
    send(sock,&msg,sizeof(int),0);
}