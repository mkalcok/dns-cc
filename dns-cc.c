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
#include "nameres.h"
#include <fcntl.h>
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

static long BITCOUNTER = 0;

//Config declarations
typedef struct server_list{
	char *server;
	struct server_list *next;
}server_list;

typedef struct conf{
    char *conf_file;
    char *input_file;
    char *output_file;
    char *name_base;
    server_list *name_server;
    char *key;
    int precision;
	int max_speed;
    } conf;

static struct conf *config;

typedef struct sample{
    int *cached_set;
    int *uncached_set;
    } sample;


static struct sample *sample_times;

int END_THREADS = 0;
pthread_mutex_t LOCK;
struct server_list *CURRENT_SERVER;

int _pow(int base, int exp){
/*
 *Recursive function, returns value of base^exp
 */
    return exp == 0 ? 1 : base * _pow(base, exp - 1); 
}

float absolute_value(float a){
/*
 *Function returns abosulte value of input variable a
 */
    if(a < 0){
        a = a*(-1);
    }
    return(a);
}

void compose_name(char *output, int seq){
/*Function creates  domain name to be queried.
 *
 *Arguments:
 *	*output - pointer to memory where domain name will be stored
 *	 seq	- sequence number that gets appended to domain name (see NOTE)
 *
 *NOTE: This is very much a dummy function for now, lacking any complexity
 */
    char charseq[10];
    sprintf(charseq, "%d", seq);
    strcat(output, config->key);
    strcat(output, charseq);
    strcat(output, config->name_base);
    int i =0;
    return;
}


void bintostr(char *output, char *binstring){
/*Function converts string of (ASCII encoded) ones and zeroes to
 * regular values (1Byte/character)
 *
 *Arguments:
 *	*output		- pointer to memmory where final string will be stored
 *	*binstring	- pointer to string (containing ones and zeroes)
 *
 * EXAMPLE:
 * 	If *binstring contains "0100000101100010", result written to *output will be "Ab"
 */
    int i, y = 0, power, dec;
    char toparse[1];
    
    for(i = 0; i < strlen(binstring); i += 8){
        power = 7;
        dec = 0;
        for(y = 0; y < 8; y++ ){
            strncpy(toparse, binstring +i + y, 1);
            if(toparse[0] == 49){
            dec += _pow(2, power);
            }
            power--;
        }
        toparse[0] = dec;
        strncat(output, toparse, 1);
    }
    return;
}

void sender_thread(int fd){
	const struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 1000};
	struct timespec rem;
	char c;
	ssize_t check;
	query_t query;
	query.domain_name = malloc(255);
	while(1){
		if(END_THREADS){
			printf("Caught exit\n");
			return;
		}

		if(pthread_mutex_trylock(&LOCK) == 0){
			check = read(fd, &c, 1);
			if(check > 0){
				if(strncmp(&c,"1",1) == 0){
					compose_name(query.domain_name, BITCOUNTER);
					query.name_server = CURRENT_SERVER->server;
					BITCOUNTER++;
					CURRENT_SERVER = CURRENT_SERVER->next;
					pthread_mutex_unlock(&LOCK);
					exec_query(&query);
					strncpy(query.domain_name, "\0", 1);
				}else{
					BITCOUNTER++;
					CURRENT_SERVER = CURRENT_SERVER->next;
					pthread_mutex_unlock(&LOCK);
				}
			}else if(check == -1 && errno == EAGAIN){
				pthread_mutex_unlock(&LOCK);
			}else{
				END_THREADS=1;
				pthread_mutex_unlock(&LOCK);
			}
		}
		nanosleep(&sleep_time, &rem);
	}
}


void join_threads(pthread_t *threads){
	int i, check;
	for(i = 0; i < 10; i++){
		//printf("WAITING FOR thread %u\n",threads[i]);
		check = pthread_join(threads[i], NULL);
		//printf("JOINED thread %u with status %d\n",threads[i], check);
	}

}

pthread_t * create_threads(int fd){
	const int count = 128;
	int s_count;
	int i;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	//pthread_t threads[10] = {0};
	pthread_t * threads = malloc(sizeof(pthread_t)*10);
	
	int arg = fd;

	for(i = 0; i < 10; i++){
		pthread_create(&threads[i], &attr, sender_thread, fd);
		//printf("Original thread %u\n",threads[i]);

	}
	
	return(threads);
}

void retrieve_msg(int desc_out, FILE *file_out){
/*Function retrieves data from DNS cache. Either file descriptor (desc_out)
 *or pointer to opened file (*file_out) must be provided to which data will be
 *written. Unused output method should be NULL, if both methods are provided,
 *file descriptor takes precedence.
 *
 * Arguments:
 *	desc_out	- file descriptor that will be used to store retrieved data
 *	*file_out	- opened FILE pointer that will be used to store retrieved data
 */
    int i, y= 0, endofmsg = 0, counter = 0;
    char bit[2];
    int byte = 0;
    char *name = calloc(255, sizeof(char));
	struct server_list *servers = config->name_server;
    while (endofmsg != 1){
        byte = 0;
		endofmsg = 1;
        for(i = y; i < y+8; i++){
            byte = byte << 1;
            strcpy(name, "");
            compose_name(name ,i);
            if (iscached(servers->server, name)){
                byte |= 1;
                endofmsg =0;
            }
			servers = servers->next;
        }
		counter++;
		printf("Bytes recieved: %d\r",counter);
		fflush(stdout);
		if (desc_out == NULL){
        	fwrite(&byte, 1, 1, file_out);
		}else{
        	write(desc_out, &byte, 1);
		}
        y += 8;
    }
	printf("\n");
    free(name);
    return;
}

void stream_to_bits(void ** args){
	int input_pipe = (int)args[0];
	int output_pipe = (int)args[1];
	int check = 0;
	int i=0, mask=128;
	char byte;
	while(1){
		check = read(input_pipe, &byte, 1);
		if (check == -1 && errno == EAGAIN){continue;}
		else if(check < 1){break;}
		mask=128;
		for(i = 0; i < 8; i++){
			if(byte & mask){
				write(output_pipe, "1",1);
				printf("1");
			}else{
				write(output_pipe, "0",1);
				printf("0");
			}
			mask = mask >> 1;
		}
		printf("\n");
	}
	close(output_pipe);
	printf("CLOSING\n");
}

void send_data(int fd){
/*XXX:DEPRECATED in favor of sender_thread()!!!!
 *
 * Function writes single byte into dns cache by executing DNS queries.
 *It utilizes pthreads to send all 8 bits of byte simultaneously
 *
 * Arguments
 * 	byte - integer value of which 8 least signiticants bits are taken
 */

	struct server_list *servers = config->name_server;
	char byte = 0;
	short check;
	query_t *query = calloc(1,sizeof(query_t));
	query->domain_name = calloc(255,sizeof(char));

	while(1){
		check = read(fd, &byte, 1);
		if (check < 1){break;}
		//pthread_t threads[8] = {0};
		int ret_t[8];
		int indexes[8];
		int i,z;
		int j=128;
		for(i = 0; i < 8; i++){
			if(byte & j){
				compose_name(query->domain_name, BITCOUNTER);
				query->name_server = servers->server;
				exec_query(query);
				strncpy(query->domain_name,"\0",1);
			}
			j = j >> 1;
			++BITCOUNTER;
			servers = servers->next;
		}
	}
    free(query->domain_name);
    free(query);
    return;
}


int iscached(char *server, char *name){
/*This function executes DNS query and emplys Weighted k-NN algorithm
 *(see http://en.wikipedia.org/wiki/K-nearest_neighbors_algorithm#k-NN_regression)
 *to determine whether domain name was previously cached or not.
 *
 *Arguments:
 *	server	- address of DNS server
 *	name	- domain name to be queried
 */

	query_t query = {.domain_name = name, .name_server = server};
    int delay = exec_query(&query);
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
/*Function returns pointer to memory with desired value stored.
 * This pointer can be then assigned to structure member.
 *
 *Arguments:
 *	*value	- data to be stored
 *	size	- size of data &value
 *
 */
    void* p = calloc(size+1, sizeof(char));
    strncpy(p, value, size);
    return p;
}

conf* init_conf(){
/*Function initialize and returns config structure
 *
 *XXX:Shouldnt be that path configurable? o.O
 */
    conf* c = malloc(sizeof(conf));
    c->conf_file = set_member("/etc/dns-cc/dns.cfg", sizeof("/etc/dns-cc/dns.cfg"));
    return c;
}

void free_servers(struct server_list *root){
	struct server_list *n = root->next;
	while(1){
		free(n->server);
		if(n->next == root){break;}
		n = n->next;
	}
	free(root->server);
	free(root->next);
	free(root);

}

void free_conf(struct conf *p){
/*Functions frees config structure and all its members
 *
 */
    free(p->conf_file);
    free(p->input_file);
    free(p->output_file);
    free(p->name_base);
    free_servers(p->name_server);
    free(p->key);
    free(p);
    return;
}

void help(){
/*This Function will display help and usage information
 *XXX:Replace dummy text with actual help
 */
    printf("\nOne day, this will be nice help :)\n");
}

void remove_blank(char *str){
/*Function removes all blank and control characters from string except final \0
 *(Used to preparse lines from config file)
 *
 * Arguments:
 *	*str	- pointer to string to be cleaned
 */
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

void test(){
	struct server_list *a = config->name_server;
	struct server_list *n,*p;
	n = a;
	while(1){
		printf("%s\n",n->server);
		n = n->next;
		sleep(1);
	}
}

void set_servers(char *servers){
	char *token;
	struct server_list *prev, *new, *first;

	token = strsep(&servers,",");
	first = calloc(1,sizeof(struct server_list));
	first->server = calloc(1,strlen(token));
	strncpy(first->server, token, strlen(token));
	first->next = first;
	config->name_server = first;
	prev =  first;

	while(token = strsep(&servers,",")){
		//#XXX:Valgrind says I'm loosing memory here
		new = calloc(1, sizeof(server_list));
		new->server = calloc(1, strlen(token));
		strncpy(new->server, token, strlen(token));
		new->next = first;
		prev->next = new;
		prev = new;
	}
}

void set_conf(char* var, char* value){
/*Function sets config structure members (if var coresponds to struct member)
 *
 *Arguments
 *	var		- struct member to be set
 *	value	- data for struct member
 */
    int value_length = (int)strlen(value);
    if(strcasecmp(var, "server") == 0){
        set_servers(value);
		CURRENT_SERVER = config->name_server;
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
    if(strcasecmp(var, "max_speed") == 0){
        config->max_speed = atoi(value);
        return;
    }


}

void read_conf_file(){
/*Function parses config file and sets config structure members
 *
 */
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
/*Function creates struct and alocates space (based on config->precision)
 * for sample time values used by iscached() to determine whether domain
 * name was cached or not
 */
    sample *s = malloc(sizeof(sample));
    s->cached_set = calloc(config->precision, sizeof(int));
    s->uncached_set = calloc(config->precision, sizeof(int));
}

void free_sample_times(){
/*Function frees structure holding sample times and its members
 *
 */
    free(sample_times->cached_set);
    free(sample_times->uncached_set);
    free(sample_times);
}


void calibrate(){
/*Function colects sample times for cached and not cached domain names.
 *Number of samples is based on config->precision defined by user
 *
 */
    sample_times = init_sample_times();
    int i = 0;
    query_t query;
    query.domain_name = calloc(255, sizeof(char));
    struct server_list *servers = config->name_server;
    for(i; i < config->precision; i++){
        query.name_server = servers->server;
        strcpy(query.domain_name, "precise.\0");
        compose_name(query.domain_name, i); 
        sample_times->uncached_set[i] = exec_query(&query);
        sample_times->cached_set[i] = exec_query(&query);
        servers = servers->next;
    }   
    free(query.domain_name);
    return;
}

void * compresor(void ** args){
/*Function calls deflate_data() from compres.h with appropriate arguments.
 *It is meant to be us in thread creation
 *
 * Arguments:
 *	arg[0]	- FILE pointer to opened input file
 *	arg[1]	- File descriptor to write end of pipe
 */
    FILE *fp = (FILE *) args[1];
    int fd = (int)args[0]; 
    deflate_data(fp, fd);
	close(fd);
    return;
}

void * decompresor(void ** args){
/*Function calls inflate_data() from compres.h with appropriate arguments.
 *It is meant to be us in thread creation
 *
 * Arguments:
 *	arg[0]	- FILE pointer to opened output file
 *	arg[1]	- File descriptor to read end of pipe
 */
    FILE *fp = (FILE *) args[0];
    int fd = (int)args[1]; 
    inflate_data(fd, fp);
    return;
}

void * file_to_fd(void ** args){
    FILE *fp = (FILE *) args[1];
    int fd = (int)args[0]; 
	int check = 0;
	char buffer[1];
    while(1){
		check = fread(&buffer,sizeof(char),1,fp);
		if(check <= 0){
			break;
			printf("ERROR WRITING TO PIPE %d\n",check);
		}
		write(fd, &buffer, 1);
	}
	close(fd);
    return;
}

void * fd_to_file(void ** args){
    FILE *fp = (FILE *) args[0];
    int fd = (int)args[1]; 
	int check = 0;
	char buffer[1];
    while(1){
		check = read(fd, &buffer, 1);
		if(check <= 0){break;}
		fwrite(&buffer,sizeof(char),1,fp);
	}
    return;
}

int main(int argc, char** argv) {
	pthread_mutexattr_t mutex_attr;
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&LOCK, &mutex_attr);
    int c, option_flags=0;
    config = init_conf();
    //XXX:Dprecated config = malloc(sizeof(conf));
    char ch[1];
    FILE *fp;
    char cfg_file[100];
    while((c = getopt(argc, argv, "DCc:s:r:")) != -1){
        switch (c){
            case 'c':
                //Redefine config file
                free(config->conf_file);
                config->conf_file = set_member(optarg, strlen(optarg));
                break;
			case 'D':
            case 'C':
                //Are we gonna (de)compress?
                option_flags += COMPRESS_FLAG;
                break;
    
            case 's':
                //program is to send data. If source file is -, open stdin
                config->input_file = set_member(optarg, strlen(optarg));
                option_flags += SEND_FLAG;
                config->output_file = set_member("\0", strlen("\0"));
	        break;

            case 'r':
				//program is to recieve data from DNS server and write them into file.
                config->output_file = set_member(optarg, strlen(optarg));
                option_flags += READ_FLAG;
                break;
            default:
                help();
                return(EXIT_FAILURE);
        }
    }
    if((option_flags & READ_FLAG) == READ_FLAG && (option_flags & SEND_FLAG) == SEND_FLAG){
        printf("You can't (-s)end and (-r)ecieve at same time, your options do not make sense\n");
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
        int data_pipe[2];
		int binary_pipe[2];

        pipe(data_pipe);
		int flags = fcntl(data_pipe, F_GETFL, 0);
		fcntl(data_pipe[0], F_SETFL, O_NONBLOCK);
		fcntl(data_pipe[1], F_SETFL, O_NONBLOCK);

		pipe(binary_pipe);
		fcntl(data_pipe[0], F_SETFL, O_NONBLOCK);
		fcntl(data_pipe[1], F_SETFL, O_NONBLOCK);


        pthread_t filestream_td = {0};
        pthread_t bitstream_td = {0};

		pthread_t *workers_td;
		workers_td = create_threads(binary_pipe[0]);

        if ((option_flags & COMPRESS_FLAG) == COMPRESS_FLAG){
            void * compres_args[2]= {data_pipe[1],fp};
            pthread_create(&filestream_td, NULL, compresor, (void *) compres_args );

			void *stream_args[2] = {data_pipe[0], binary_pipe[1]};
			pthread_create(&bitstream_td, NULL, stream_to_bits, (void *) stream_args);

			pthread_join(filestream_td, NULL);
			pthread_join(bitstream_td, NULL);
			close(data_pipe[0]);
        }else{
			void *args[2] = {data_pipe[1],fp};
            pthread_create(&filestream_td, NULL, file_to_fd, (void *) args );

			void *stream_args[2] = {data_pipe[0], binary_pipe[1]};
			pthread_create(&bitstream_td, NULL, stream_to_bits, (void *) stream_args);
	
			pthread_join(filestream_td, NULL);
			pthread_join(bitstream_td, NULL);
			close(data_pipe[0]);
        }
			
		join_threads(workers_td);

        fclose(fp);
        printf("Data sent successfuly\n");
        free(byte);
        free_conf(config);
        return(EXIT_SUCCESS);
    }
    if((option_flags & READ_FLAG) == READ_FLAG){
    	FILE *fp = fopen(config->output_file,"wb");
        printf("Retrieving data...\n");
        calibrate();

        int fd[2];
        pipe(fd);
        pthread_t td = {0};
        if ((option_flags & COMPRESS_FLAG) == COMPRESS_FLAG){
            void * decompres_args[2]= {fp,fd[0]};
            pthread_create(&td, NULL, decompresor, (void *) decompres_args );
			retrieve_msg(fd[1], NULL);
			close(fd[1]);
            pthread_join(td, NULL);
		}else{
			void * args[2] = {fp, fd[0]};
            pthread_create(&td, NULL, fd_to_file, (void *) args );
        	retrieve_msg(fd[1],NULL);
			close(fd[1]);
            pthread_join(td, NULL);
		}
		close(fd[0]);
		printf("Done Reading\n");
		fclose(fp);
        free_sample_times();
        free_conf(config);
        return(EXIT_SUCCESS);
    } 
    help();
    /*printf("input_file %s\n", config->input_file);
    printf("name_server %s\n", config->name_server);
    printf("name_base %s\n", config->name_base);
    printf("key %s\n", config->key);*/
    free_conf(config);
    return (EXIT_SUCCESS);
}
