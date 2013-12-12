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

/*
 * 
 */
int main(int argc, char** argv) {
   
    char s[100], substring[13], check[13]=";; Query time", *time, *units;
    int i = 0, milisec=0;
    FILE *input;

    input = popen("dig @208.67.222.222 www.fasjdaslkmkls.sk +time=10", "r");
    while(fgets(s, 100, input) != NULL){
        if(strcmp(strncpy(substring, s, 13), check) == 0){
            strcpy(substring, s+15);
            time = strtok(substring, " ");
            units = strtok(NULL, " ");
            break;
        }
    }
    milisec = strtol(time, NULL, 0);
    if(strcmp(units, "s") == 0){
        milisec *= 1000;
    }
    printf("%d\n",milisec);
    return (EXIT_SUCCESS);
}

