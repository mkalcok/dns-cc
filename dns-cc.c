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
struct dataByte{
   unsigned short i[8];
};
int getDelay(char *server, char *name){
    
    char s[100], substring[13], check[13]=";; Query time", *time, *units;
    int i = 0, milisec=0;
    FILE *input;
    input = popen("dig @208.67.222.222 www.fasjdaasdasdslkmkls.sk", "r");
    while(fgets(s, 100, input) != NULL){
        if(strncmp(strncpy(substring, s, 13), check, 13) == 0){
            strcpy(substring, s+15);
            time = strtok(substring, " ");
            units = strtok(NULL, " ");
            break;
        }
    }
    pclose(input);
    milisec = strtol(time, NULL, 0);
    if(strcmp(units, "s") == 0){
        milisec *= 1000;
    }
    return milisec;
}

int isCached(char *server, char *name){
    int i, initial, control[5], treshold, cached;
    initial = getDelay(server, name);
    treshold = initial / 5;
        printf("initial: %d\ntreshold: %d\n",initial, treshold);
    for(i = 0; i<(sizeof(control) / sizeof(control[0])); i++){
        control[i] = getDelay(server, name);
        printf("control[%d]: %d\n",i , control[i]);
        if (control[i] < (initial - treshold)){
            cached = 0;
        }else{cached = 1;}
    }
    return cached;
}

int main(int argc, char** argv) {
    
    
    printf("Is cached?: %d\n", isCached("lorem","ipsum"));
    //printf("%lu\n",sizeof(n));
    return (EXIT_SUCCESS);
}

