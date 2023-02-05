

#include "psar.h"
#define INDICE 8190
#define TAILLE_MODIF 10


int main(int argc, char *argv[]){
    
    
    if(argc != 2){
        printf("il faut passer addr IPV4 en arguument");
        exit(1);
    }

    int sock =-1;
    int *adr = (int*)InitSlave(argv[1],&sock);


    // permet d'avoir une adresse aligné
    char* page1 = (char*)adr+((INDICE/4)*4);
    
    // nbpagePris et compare sont utiliser pour relacher TOUTES les pages prises
    int nbpagePris = 0;
    int compare = ((INDICE)/4096);

    // la taille a modifier pour chaque page
    int tailleModif[10]={0};
    // l'endoit ou commence les modification de la page
    int taboffset[10]={0};


    // le tableau pour stocké la partie de la page a modifier
    int buffer2[TAILLE_MODIF];

    // read + unlock-------------------------------------
    for (int i =0; i< TAILLE_MODIF; i++){
        buffer2[i]= adr[INDICE/4+i]+8;
        if (((INDICE+i*4)/4096) == compare){            
            nbpagePris++;
            compare++;

        }
    }
    // relache toute les pages prises
    char* tmpPage1 = page1;
    for (int i =0; i< nbpagePris; i++){
        tmpPage1 = tmpPage1+(i*4096);
        req_unlock_read(&tmpPage1,sock);

    }


    //ECRITURE

    compare = ((INDICE)/4096);
    nbpagePris = 0;

    // write + unlock
    for (int i =0; i< TAILLE_MODIF; i++){
        //ecriture
        adr[INDICE/4+i] = buffer2[i];

        // le if est executer a chaque ecriture sur une NOUVELLE page
        if (((INDICE+i*4)/4096) == compare){
            // pour initialiser le tableau des offsets
            taboffset[nbpagePris]=(i*4);

            // c'est la 2eme fois qu'il change de page 
            if (nbpagePris==1){
                tailleModif[nbpagePris-1]=i;
            }
            // c'est au moins la 3eme fois qu'il change de page
            if (nbpagePris>1 ){
                tailleModif[nbpagePris-1]= TAILLE_MODIF - tailleModif[nbpagePris-2];
            }       

            nbpagePris++;
            compare++;
        }
    }
    
    // mettre la derniere taille de donnees modifier dans le tableau 
    if (nbpagePris>1){
        tailleModif[nbpagePris-1]= TAILLE_MODIF - tailleModif[nbpagePris-2];
    }else{
        tailleModif[nbpagePris-1]=TAILLE_MODIF;
    }

    sleep(5);
    // relache toute les pages prises
    tmpPage1 = page1;
    for (int i =0; i< nbpagePris; i++){
        tmpPage1 = tmpPage1+taboffset[i];
        sleep(2);
        req_unlock_write(&tmpPage1,tailleModif[i],sock);

    }


    // sleep(50);
    end_sock(sock);
    printf("fin test general\n");
    return 0;
}