/* 
 * File:   main.c
 * Author: kauchman
 *
 * Created on December 11, 2013, 9:53 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/*
 * 
 */


char* strtobin(char *input){
int i, y, dec, bin;
char toparse[1];
char *output = malloc((strlen(input) * 8)+10);

printf("%s\n", output);
for(i = 0; i < strlen(input) -1; i++){
        strncpy(toparse, input + i, 1);
        dec = toparse[0];
        printf("%c - %d - ", toparse[0], dec);
        
        for(y = 7; y >=0; y--){
            bin = dec >> y;
            if(bin & 1){
                printf("1");
                strcat(output, "1");
            }else{
                printf("0");
                strcat(output, "0");
            }
        }
        
        printf("\n");
}
return(output);
}

int getdelay(char *server, char *name){                                         /*funkcia vyparsuje delay odpovede DNS servera*/
    
    char s[100], substring[13], check[13]=";; Query time", *time, *units;       /*inicializacia uvodnych premennych*/
    int i = 0, milisec=0;                                                       /*inicializacia uvodnych premennych*/
    FILE *input;                                                
   
    input = popen("dig @208.67.222.222 www.fasjdaasdasdslkmkls.sk", "r");       /*do input je vlozeny vypis prikazu dig*/
    
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

int iscached(char *server, char *name){                         /*Funkcia zisti na zaklade odozvy odpovede ci je zaznam v cache*/
    int i, initial, control[5], treshold, cached;               /*inicializacia premennych. i - pomocna premenna cyklu; initial - delay prveho dotazu; control[] - delay x kontrolnych dotazov; treshhold - odchylka control[] od initial ktora urcuje pritomnost v cache; cached - 0 alebo 1, hodnota je navratovou hodnotou funkcie*/
    
    initial = getdelay(server, name);                           /*ziskanie prveho delayu*/
    treshold = initial / 5;                                     /* threshhold je 20% z initial*/
    
    //printf("initial: %d\ntreshold: %d\n",initial, treshold);
    for(i = 0; i<(sizeof(control) / sizeof(control[0])); i++){
        control[i] = getdelay(server, name);                    /*ziskanie kontrolneho delayu*/
        //printf("control[%d]: %d\n",i , control[i]);
        if (control[i] < (initial - treshold)){                 /*kontrola ci je control[] mensi ako initial minus treshhold*/
            cached = 0;
        }else{cached = 1;}
    }
    return cached;
}

int main(int argc, char** argv) {
    //char *server = argv[1];
    char message[140];
    FILE *fp;
    
    fp = fopen("sample.msg", "r");
    if (fp == NULL){
        printf("cant open file\n");
        return(0);
    }
    
    while(fgets(message, sizeof(message), fp) != NULL){
    fputs(message, stdout);
    }
    
    fclose(fp);
    char *binmessage;
    binmessage = strtobin(message);
    printf("%s\n", binmessage);
    return (EXIT_SUCCESS);
}
