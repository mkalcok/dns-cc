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
#include <string.h>
#include <pthread.h>
#include "compress.h"
#include <errno.h>
//#include <ares.h>
/*
 * 
 */

//Define argument flags
#define CONF_FLAG  1
#define SEND_FLAG  2
#define READ_FLAG  4
#define COMPRESS_FLAG  8

typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;

static long bitcounter_g = 0;

//Config declarations
typedef struct conf{
    char *conf_file;
    char *input_file;
    char *output_file;
    char *name_base;
    char *name_server;
    char *key;
    int precision;
    } conf;

static struct conf *config;

typedef struct sample{
    int *cached_set;
    int *uncached_set;
    } sample;

static struct sample *sample_times;

int _pow(int base, int exp){                                                    /*rekurzivna funkcia na vypocet mocniny base^exp*/
    return exp == 0 ? 1 : base * _pow(base, exp - 1); 
}

float absolute_value(float a){
    if(a < 0){
        a = a*(-1);
    }
    return(a);
}

void compose_name(char *output, int seq){                                 /*Funkcia generuje dns meno, bude komplexnejsia zatial je to len zakladny navrh kvoli testu funkcnosti*/
    char charseq[10];                                                           /*inicializacia uvodnych premennych*/
    sprintf(charseq, "%d", seq);                                                /*sekvencne cislo ktore bolo ako argument funkcie sa konvertuje int -> string*/
    
    strcat(output, config->key);
    strcat(output, charseq);
    strcat(output, config->name_base);
    int i =0;
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

void retrieve_msg(){
    int i, y= 0, endofmsg = 0;
    char bit[2];
    FILE *fp = fopen(config->output_file,"wb");
    int byte = 0;
    char *name = calloc(255, sizeof(char));
    while (endofmsg != 1){
        byte = 0;
	endofmsg = 1;
        for(i = y; i < y+8; i++){
            byte = byte << 1;
            strcpy(name, "");
            compose_name(name ,i);
            if (iscached(config->name_server, name)){
                byte |= 1;
                endofmsg =0;
            }
        }
        fwrite(&byte, 1, 1, fp);
        y += 8;
    }
    free(name);
    fclose(fp);
    return;
}



void retrieve_msg2(){	        						/*Funkcia  cita spravu z dns mien odvodenych z char *key */
    int i, y= 0, endofmsg = 0;							/*inicializacia uvodnych premennych*/
    char bit[2];
    char *binmessage = calloc(16000, sizeof(char));
    char *name = calloc(255, sizeof(char));
    char *message = calloc(200, sizeof(char));
    while (endofmsg != 1){							/*While cyklus prechadza jednotlive bajty spravy zatial co vnoreny for cyklus prechadza tieto bajty po bitoch, opakuje sa az kym for cyklus nenajde bajt plny nulovych bitov (sprava skoncila)*/
	endofmsg = 1;								/*na zaciatku kazdeho cyklu predpokladame ze tento bajt je posledny*/
        for(i = y; i < y+8; i++){    						/*for cyklus prejde 8 bitov ktore zapise do vystupneho binarneho retazca binstring, ak narazi na '1' zapise do premennej endofmsg hodnotu 0 cim indikuje ze nejde o posledny bajt*/
            strcpy(name, "");                                                   /*vynulovanie retazca pri kazdej iteracii cyklu*/
            compose_name(name ,i);                                               /*z kluca a cisla iteracie vygenerujeme plne dns meno funkciou compose_name()*/
            sprintf(bit, "%d", iscached(config->name_server, name));		/*DNS zaznam sa otestuje ci je zacacheovany a vysledok sa pretypuje int -> string*/
            //printf("%c",bit[0]);
            strcat(binmessage, bit);						/*vysledok sa prilepy na koniec retazca binmessage*/
	    if(bit[0] == '1'){							/*ak je vysledok '1', indikuje to ze nejde o posledny bajt a podla toho sa nastavi aj premenna endofmsg */
	    endofmsg =0;
	    }
        }
    //printf(" ");
    y += 8; 									/*pocitadlo sa posunien a zaciatok dalsieho bajtu*/
    }
    bintostr(message, binmessage);						/*sprava sa posle funkcii bintostr() aby sa prelozil bitovy retazec na text a vypise sa*/
    printf("%s \n",message);
    free(binmessage);
    free(name);
    free(message);
    return;										/*uvolni sa alokovana pamat*/
}

void * bulk_test(void * q){
    printf("%s\n",(char *) q);
    //system(q);
    return;
}

void send_data(int byte){   
    pthread_t threads[8] = {0};
    int ret_t[8];
    int indexes[8];
    int i,z;
    int j=128;
    char space[2] = " \0";
    char cmd[25] = "dig @";
    char pipe[15] = " > /dev/null";
    char **query = malloc(8*(sizeof(char *))); 
    for(i = 0; i < 8; i++){
        query[i]= calloc(255, sizeof(char));

    }
    for(i = 0; i < 8; i++){
        strcpy(query[i], cmd);
        strcat(query[i], config->name_server);
        strcat(query[i], space);
        if(byte & j){
            compose_name(query[i], bitcounter_g);
	    strcat(query[i], pipe);
            indexes[i] = i;
            //pthread_create(&threads[i], NULL, bulk_test,&i);
            pthread_create(&threads[i], NULL, bulk_test , query[i]);
            //system(query);
        }
        j = j >> 1;
        ++bitcounter_g;
    }
    //printf("\r%ld bytes sent",(bitcounter_g/8));
    //fflush(stdout);
    
    for(z=0;z < 8; z++){
        pthread_join(threads[z], NULL);
    }

    for(i = 0; i < 8; i++){
        free(query[i]);
    }
    free(query);
    
    return;
}

void strtobin(char *output, char *input){                                                    /*funkcia transformuje string na binarny retazec*/
    int i, y, dec, bin;                                                             /*inicializacia premennych*/
    char toparse[1];
    for(i = 0; i < strlen(input)-1; i++){                                          /*cyklus prechadza string po znakoch*/
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


//TODO:Get server and name from config!
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
    //printf("%d", milisec);
    return milisec;
}

//Modified kNN algorithm
int iscached(char *server, char *name){
    int delay = getdelay(server, name);
    float distance = 0;
    float cached_score = 0;
    float uncached_score = 0;
    int i;

    for(i=0; i < config->precision; i++){
        if(delay == sample_times->cached_set[i]){continue;}
        distance = 1.0/(delay - sample_times->cached_set[i]);
        cached_score += absolute_value(distance);

        if(delay == sample_times->uncached_set[i]){continue;}
        distance = 1.0/(delay - sample_times->uncached_set[i]);
        uncached_score += absolute_value(distance);
    }
    if(cached_score > uncached_score){
        return(1);
    }else{
        return(0);
    }
    
}


void* set_member(void  *value, size_t size){
    void* p = calloc(size+1, sizeof(char));
    strncpy(p, value, size);
    return p;
}

conf* init_conf(){
    conf* c = malloc(sizeof(conf));
    c->conf_file = set_member("/etc/dns-cc/dns.cfg", sizeof("/etc/dns-cc/dns.cfg"));
    return c;
}

void free_conf(struct conf *p){
    free(p->conf_file);
    free(p->input_file);
    free(p->output_file);
    free(p->name_base);
    free(p->name_server);
    free(p->key);
    free(p);
    return;
}

void help(){
    printf("\nOne day, this will be nice help :)\n");
}

void remove_blank(char *str){
    char *clean_str = calloc(strlen(str), sizeof(char));
    char *buf = malloc(1);
    int i = 0;
    while(1){
        memcpy(buf, &str[i], 1);
        if(strcmp(buf, "\0") == 0){break;}
        if(iscntrl(buf[0]) != 0){++i; continue;}
        if(isblank(buf[0]) == 0){strncat(clean_str, buf, 1);}
        ++i;
    }
    strcat(clean_str, "\0");
    strcpy(str, clean_str);
    free(clean_str);
    free(buf);
    return;
}

void set_conf(char* var, char* value){
    int value_length = (int)strlen(value);
    if(strcasecmp(var, "server") == 0){
        config->name_server = set_member(value, value_length);
        return;
    }
    if(strcasecmp(var, "base_name") == 0){
        config->name_base = set_member(value, value_length);
        return;
    }
    if(strcasecmp(var, "key") == 0){
        config->key = set_member(value, value_length);
        return;
    }
    if(strcasecmp(var, "precision") == 0){
        config->precision = atoi(value);
        return;
    }



}

void read_conf_file(){
    FILE *fp;
    fp = fopen(config->conf_file, "r");
    size_t line_length = 200;
    char *var;
    char *line = malloc(line_length);
    ssize_t read;

    if (fp == NULL){
        printf("Can't open configuration file. %s\n", config->conf_file);
        exit(0);
    }

    printf("Using config file %s\n", config->conf_file);
    
    while ( (read = getline(&line, &line_length, fp)) != -1){
    if(strncmp(line,"#",1) == 0 || strncmp(line, "\n", 1) == 0){continue;}
    line_length = (int)strlen(line);
    
    remove_blank(line);
    char *p = strdup(line);
    var = strsep(&p, "=");
    set_conf(var, p);
    //Freeing var because it points to original *p where space was allocated by strdup
    free(var);
    }
    fclose(fp);
    free(line);
}

sample* init_sample_times(){
    sample *s = malloc(sizeof(sample));
    s->cached_set = calloc(config->precision, sizeof(int));
    s->uncached_set = calloc(config->precision, sizeof(int));
}

void free_sample_times(){
    free(sample_times->cached_set);
    free(sample_times->uncached_set);
    free(sample_times);
}


void calibrate(){
    sample_times = init_sample_times();
    int i = 0;
    char *name = calloc(255, sizeof(char));
    for(i; i < config->precision; i++){
        strcpy(name, "precise.\0");
        compose_name(name, i);
        sample_times->uncached_set[i] = getdelay(config->name_server, name);
        sample_times->cached_set[i] = getdelay(config->name_server, name);
    }
    free(name);
    return;
}

void * compresor(void ** args){
    FILE *fp = (FILE *) args[1];
    int fd = args[0]; 
    deflate_data(fp, fd);
    close(fd);
    return;
}

void test(FILE *fp){
    /*int x;
    int fd[2],e;
    void *p = calloc(sizeof(char),1);
    FILE *ofp = fopen("/tmp/deflate.msg","w");
    pthread_t td = {0};
    pipe(fd);

    pthread_create(&td, NULL, compresor, (int *) fd[1]);
    while (1) {
        e = read(fd[0],p,1);
        fwrite(p,1,1,ofp);
        if(e == 0){break;}
        //sleep(1);
    }
    close(fd[0]);
    fclose(ofp);
    pthread_join(td, NULL);
    exit(0);*/
    return;
    

}


int main(int argc, char** argv) {
    int c, option_flags=0;
    config = init_conf();
    //XXX:Dprecated config = malloc(sizeof(conf));
    char ch[1];
    FILE *fp;
    char cfg_file[100];
    while((c = getopt(argc, argv, "Cc:s:r:")) != -1){
        switch (c){
            case 'c':
                //Redefine config file
                free(config->conf_file);
                config->conf_file = set_member(optarg, strlen(optarg));
                break;

            case 'C':
                //Are we gonna compress?
                option_flags += COMPRESS_FLAG;
                break;
    
            case 's':
                //program is to send data. If source file is -, open stdin
                //XXX:Je takyto STDIN standard?
                config->input_file = set_member(optarg, strlen(optarg));
                option_flags += SEND_FLAG;
	        break;

            case 'r':
                config->output_file = set_member(optarg, strlen(optarg));
                option_flags += READ_FLAG;
                break;
            default:
                help();
                return(EXIT_FAILURE);
        }
    }
    if((option_flags & READ_FLAG) == READ_FLAG && (option_flags & SEND_FLAG) == SEND_FLAG){
        printf("You can either send or recieve, your options do not make sense\n");
        help();
        return(EXIT_FAILURE);
    }
    
    read_conf_file();
    if((option_flags & SEND_FLAG) == SEND_FLAG){
        if (strcmp(config->input_file,"-") == 0){
            printf("Type in your message (end with ^D):  ");
            fp = stdin;
            }else{
            fp = fopen(config->input_file, "r");
            }

        if (fp == NULL){
            printf("Can't open input file: %s\n", config->input_file);
            return(EXIT_FAILURE);
        }
        printf("Sending Data...\n");
        int *byte = calloc(1,sizeof(int));
        int check = 1;
        if ((option_flags & COMPRESS_FLAG) == COMPRESS_FLAG){
            int fd[2];
            pthread_t td = {0};
            pipe(fd);
            void * compres_args[2]= {fd[1],fp};

            pthread_create(&td, NULL, compresor, (void *) compres_args );
            pthread_join(td, NULL);
            while(1){
                check = read(fd[0],byte, 1);
                if (check < 1){break;}
                send_data(byte[0]);
            }
            close(fd[1]);
        }else{
            while(check){
                check = fread(byte, 1, 1, fp);
                send_data(byte[0]);
            }
        }

        fclose(fp);
        printf("Data sent successfuly\n");
        free(byte);
        free_conf(config);
        return(EXIT_SUCCESS);
    }
    if((option_flags & READ_FLAG) == READ_FLAG){
        printf("Retrieving data...\n");
        calibrate();
        retrieve_msg();
        free_sample_times();
        free_conf(config);
        return(EXIT_SUCCESS);
    } 
    help();
    free_conf(config);
    /*printf("input_file %s", config->input_file);
    printf("name_server %s", config->name_server);
    printf("name_base %s", config->name_base);
    printf("key %s", config->key);*/
    return (EXIT_SUCCESS);
}
