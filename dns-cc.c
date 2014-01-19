/* 
 * File:   main.c
 * Author: kauchman
 *
 * Created on December 11, 2013, 9:53 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/*
 * 
 */

int _pow(int base, int exp){                                                    /*rekurzivna funkcia na vypocet mocniny base^exp*/
    return exp == 0 ? 1 : base * _pow(base, exp - 1); 
}

void getname(char *output, char *key, int seq){                                 /*Funkcia generuje dns meno, bude komplexnejsia zatial je to len zakladny navrh kvoli testu funkcnosti*/
    char charseq[10];                                                           /*inicializacia uvodnych premennych*/
    char domain2[10] = "www.";
    char tld[5] = ".sk";
    sprintf(charseq, "%d", seq);                                                /*sekvencne cislo ktore bolo ako argument funkcie sa konvertuje int -> string*/
    
    strcat(output, domain2);                                                    /*strcat posklada vysledne meno*/
    strcat(output, key);
    strcat(output, charseq);
    strcat(output, tld);
    
    return;
}
void bintostr(char *output, char *binstring){                                                /*funkcia transformuje binarny retazec do stringu*/
    int i, y = 0, power, dec;                                                   /*inicializacia premennych*/
    char toparse[1];
    
    for(i = 0; i < strlen(binstring); i += 8){                                  /*cyklus prechadza cely binarny retazec s krokom dlzky 8 (8bitov = 1bajt = 1char)*/
        power = 7;                                                              /*pri kazdom kroku sa resetuje premenna power. tato sa pouziva pri ziskavani desiatkovej hodnoty z binarneho tvaru*/
        dec = 0;                                                                /*pomocna premenna na ukladanie desiatkovej hodnoty pismena*/
        for(y = 0; y < 8; y++ ){                                                /*cyklus prechadza 8bitov binarneho retazca*/
            strncpy(toparse, binstring +i + y, 1);                              /*vyberie sa jeden bit*/
            if(toparse[0] == 49){                                               /*ak je vybrany bit 1 (49 je ascii hodnota znaku '1')*/
            dec += _pow(2, power);                                              /*k desiatkovej hodnote sa pripocita mocnina dvojky so sucasnym exponentom*/
            }
            power--;                                                            /*bitovy retazec sa cita od najvyznamnejsieho miesta preto exponent zacina na 7 a v kazdom cykle sa znizuje*/
        }
        toparse[0] = dec;                                                       /*konverzia int to char*/
        strncat(output, toparse, 1);                                            /*prilepenie vysledneho znaku na koniec retazca*/
    }
    return;
}

void readmsg(char *key){							/*Funkcia  cita spravu z dns mien odvodenych z char *key */
    int i, y= 0, endofmsg = 0;							/*inicializacia uvodnych premennych*/
    char bit[2];
    char *binmessage = (char *) malloc(1600);
    char *name = (char *) malloc((sizeof(char)) * 255);
    char *message = (char *) malloc(200);
    while (endofmsg != 1){							/*While cyklus prechadza jednotlive bajty spravy zatial co vnoreny for cyklus prechadza tieto bajty po bitoch, opakuje sa az kym for cyklus nenajde bajt plny nulovych bitov (sprava skoncila)*/
	endofmsg = 1;								/*na zaciatku kazdeho cyklu predpokladame ze tento bajt je posledny*/
        for(i = y; i < y+8; i++){    						/*for cyklus prejde 8 bitov ktore zapise do vystupneho binarneho retazca binstring, ak narazi na '1' zapise do premennej endofmsg hodnotu 0 cim indikuje ze nejde o posledny bajt*/
            strcpy(name, "");                                                   /*vynulovanie retazca pri kazdej iteracii cyklu*/
            getname(name ,key,i);                                               /*z kluca a cisla iteracie vygenerujeme plne dns meno funkciou getname()*/
            sprintf(bit, "%d", iscached("208.67.222.222", name));		/*DNS zaznam sa otestuje ci je zacacheovany a vysledok sa pretypuje int -> string*/
            //printf("%s %c \n",name, bit[0]);
            strcat(binmessage, bit);						/*vysledok sa prilepy na koniec retazca binmessage*/
	    if(bit[0] == '1'){							/*ak je vysledok '1', indikuje to ze nejde o posledny bajt a podla toho sa nastavi aj premenna endofmsg */
	    endofmsg =0;
	    }
        }
   y += 8; 									/*pocitadlo sa posunien a zaciatok dalsieho bajtu*/
   }
   bintostr(message, binmessage);						/*sprava sa posle funkcii bintostr() aby sa prelozil bitovy retazec na text a vypise sa*/
   printf("%s \n",message);
   free(binmessage);
   free(name);
   free(message);
   return;										/*uvolni sa alokovana pamat*/
}

void sendmsg(char *binmessage, char *key ){					/*vunkcia zapise bitovu spravu binmessage na DNS server podla kluca key*/
    int i;									/*inicializacia uvodnych premennych*/
    char bit[1];
    char cmd[25] = "dig @208.67.222.222 ";					/*samotny prikaz*/
    char pipe[15] = " > /dev/null";						/*pipe sa prilepy nakoniec aby sme sa zbavily textoveho vystupu programu*/
    char *query = (char *) malloc((sizeof(char)) * 255);
    
    for(i = 0; i < strlen(binmessage); i++){					/*binmessage sa prechadza po bitoch a kde sa narazi na hodnotu jedna tam sa spravi DNS dotaz*/
        strcpy(query, cmd);
        strncpy(bit, binmessage + i, 1);
        if(bit[0] == '1'){
            getname(query, key, i);
	    strcat(query, pipe);
            system(query);
        }
    }
   free(query);
   return;
}

void strtobin(char *output, char *input){                                                    /*funkcia transformuje string na binarny retazec*/
    int i, y, dec, bin;                                                             /*inicializacia premennych*/
    char toparse[1];

    for(i = 0; i < strlen(input) -1; i++){                                          /*cyklus prechadza string po znakoch*/
        strncpy(toparse, input + i, 1);                                         /*vyber jedneho pismena*/
        dec = toparse[0];                                                       /*char to int*/
        //printf("%c - %d - ", toparse[0], dec);        
        
        for(y = 7; y >=0; y--){                                                 /*prevod desiatkoveho cisla na binarny string, jeden char na 8 bitov*/
            bin = dec >> y;                                                     /*lsr o 'y' miest*/
            if(bin & 1){                                                        /*ak je po shifte na poslednom mieste 1, pripise sa k stringu 1, ak nie tak 0*/
                //printf("1");
                strcat(output, "1");
            }else{
                //printf("0");
                strcat(output, "0");
            }
        }
        
        //printf("\n");
   }
   return;
}

int getdelay(char *server, char *name){                                         /*funkcia vyparsuje delay odpovede DNS servera*/
    
    char s[100], substring[13], check[13]=";; Query time", *time, *units;       /*inicializacia uvodnych premennych*/
    char cmd[255] = "dig @";
    int i = 0, milisec=0;                                                       /*inicializacia uvodnych premennych*/
    FILE *input;                                                
    
    strcat(cmd, server);
    strcat(cmd, " ");
    strcat(cmd, name);
    input = popen(cmd, "r");       /*do input je vlozeny vypis prikazu dig*/
    
    while(fgets(s, 100, input) != NULL){                                        /*vyparsovanie delayu a jednotiek casu*/
        if(strncmp(strncpy(substring, s, 13), check, 13) == 0){
            strcpy(substring, s+15);
            time = strtok(substring, " ");
            units = strtok(NULL, " ");
            break;
        }
    }
    
    pclose(input);                                                              /*zatvorenie suboru input*/
    
    milisec = strtol(time, NULL, 0);                                            /*prevod vyparsovaneho stringu na integer*/
    if(strcmp(units, "s") == 0){                                                /*prevod jednotiek v pripade ze je delay v sekundach*/
        milisec *= 1000;
    }
    return milisec;
}

int iscached(char *server, char *name){                                         /*Funkcia zisti na zaklade odozvy odpovede ci je zaznam v cache*/
    int i, initial, control[5], treshold, cached;                               /*inicializacia premennych. i - pomocna premenna cyklu; initial - delay prveho dotazu; control[] - delay x kontrolnych dotazov; treshhold - odchylka control[] od initial ktora urcuje pritomnost v cache; cached - 0 alebo 1, hodnota je navratovou hodnotou funkcie*/
    
    initial = getdelay(server, name);                                           /*ziskanie prveho delayu*/
    treshold = initial / 5;                                                     /* threshhold je 20% z initial*/
    
    //printf("initial: %d\ntreshold: %d\n",initial, treshold);
    for(i = 0; i<(sizeof(control) / sizeof(control[0])); i++){
        control[i] = getdelay(server, name);                                    /*ziskanie kontrolneho delayu*/
        //printf("control[%d]: %d\n",i , control[i]);
        if (control[i] < (initial - treshold)){                                 /*kontrola ci je control[] mensi ako initial minus treshhold*/
            cached = 0;
        }else{cached = 1;}
    }
    return cached;
}

int main(int argc, char** argv) {
    int c;
    char message[140], name[250];
    FILE *fp;
    while((c = getopt(argc, argv, "sr")) != -1){
        switch (c){
            case 's':
   	        fp = fopen("sample.msg", "r");
    	        if (fp == NULL){
       		    printf("cant open file\n");
            	    return(0);
   	    	}	
    
    	  	while(fgets(message, sizeof(message), fp) != NULL){}
     	    	fclose(fp);
    	    
	    	char *binmessage = (char *) malloc(sizeof(message)*8);
    	    	strtobin(binmessage ,message);
    
    	    	printf("Vlozte dns meno (len meno, ziadne tld ani www):");
    	    	scanf("%s", name);
	    	sendmsg(binmessage, name);
		free(binmessage);
	    break;
            case 'r':
		printf("Vlozte dns meno (len meno, ziadne tld ani www):");
    	    	scanf("%s", name);
		readmsg(name);
                break;                      
        }
   }
    return (EXIT_SUCCESS);
}
