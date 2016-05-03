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
#include <pthread.h>
#include "libs/compress.h"
#include "libs/nameres.h"
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include "libs/display.h"
#include "libs/crypto.h"
#include "libs/fec.h"
#include "libs/crc.h"
#include <errno.h>
#include <time.h>
/*
 * 
 */

//Define argument flags
//#define CONF_FLAG  1
#define SEND_FLAG  2
#define READ_FLAG  4
#define COMPRESS_FLAG  8
#define INTERACTIVE_FLAG 16
#define ENCRYPTION_FLAG 32
#define FEC_FLAG 64

//Define header parameters
#define HEADER_LEN_SIZE 31
#define HEADER_COM_FLAG 1
#define HEADER_ENC_FLAG 1
#define HEADER_FEC_FLAG 1
#define HEADER_CRC_FLAG 1
#define HEADER_LEN_CRC 2

//Program options
uint32_t GLOBAL_OPTIONS = 0;

//Statuses used in interactive mode
char STAT_IDLE[5] = "Idle";
char STAT_SENDING[16] = "Sending message";
char STAT_SENT_OK[13] = "Message sent";
char STAT_RECIEVING[18] = "Recieving message";

/*Domain name prefixes used in interactive mode
 * to separate user data and singalisation*/
char USER1_SYNC[6] = "1sync";
char USER2_SYNC[6] = "2sync";
char USER1_PREFIX[6] = "1user";
char USER2_PREFIX[6] = "2user";

//Global variables used by S_ender in interactive mode
uint32_t S_BIT_INDEX = 64;
uint64_t S_DATA_LENGTH;
uint64_t S_HEADER;
uint32_t S_HEADER_INDEX = 0;
uint64_t S_HEADER_INDEX_REL = 0;
uint32_t S_HEADER_INDEX_STOP = 63;
uint16_t S_HEADER_CRC;

//Global variables used by R_eceiver in interactive mode
uint32_t R_BIT_INDEX = 64;
uint64_t R_DATA_LENGTH;
uint32_t R_DATA_END = 0;
uint32_t R_HEADER_INDEX = 0;

//Global variables to hold index of last used synchronization bit
uint32_t BUDDY_SYNC_INDEX = 0;
uint32_t MY_SYNC_INDEX = 0;

/*Global variable used by bin_to_file method to put bits back together
 *in correct order.*/
unsigned long REBUILD_INDEX = 64;

/*Verbosity level
* 0 - None (Default)
* 1 - Debug
*/
unsigned char VERBOSITY = 0;

//Structure to hold assigned prefixes in interactive mode
typedef struct prefix_config_t {
    char *my_sync;
    char *buddy_sync;
    char *my_data_prefix;
    char *buddy_data_prefix;
}sync_target_t;

static struct prefix_config_t prefix_config = {
        .my_sync = NULL,
        .buddy_sync = NULL,
        .my_data_prefix = NULL,
        .buddy_data_prefix = NULL
};

//Circular linked list of DNS servers
typedef struct server_list {
    char *server;
    struct server_list *next;
    bool root;
} server_list;

//Global configuration variables
typedef struct conf {
    char *conf_file;
    char *input_file;
    char *output_file;
    char *name_base;
    server_list *name_server;
    char *passphrase;
    int precision;
    int max_speed;

    int (*method)(struct query_t *);
} conf;

static struct conf *config;

//Structure to hold sample times learned during calibration
typedef struct sample {
    int *cached_set;
    int *uncached_set;
} sample;


static struct sample *sample_times;

int ttl_reference;

//Global variables used to control loops in separate threads
int END_WORKERS = 0;
int END_INTERACTIVE = 0;

//Mutexes for multi-threading synchronization
pthread_mutex_t SENDER_LOCK;
pthread_mutex_t RETRIEVER_LOCK;

/*Pointer to a server that should be used for reading/writing
 *of next bit*/
struct server_list *CURRENT_SERVER;

//Global statistics
typedef struct statistic {
    unsigned long total_bits_transmited;
    unsigned long total_data_bits;
    float start_time;
    float end_time;
    unsigned int errors;
} statistic;

statistic global_stat = {0, 0, 0, 0, 0};

int _pow(int base, int exp) {
/*
 * Recursive function, returns value of base^exp
 * Arguments:
 *      (int) base - base number
 *      (int) exp  - exponent
 */
    return exp == 0 ? 1 : base * _pow(base, exp - 1);
}

double absolute_value(double a) {
/*
 *Function returns absolute value of input variable a
 */
    if (a < 0) {
        a = a * (-1);
    }
    return (a);
}

void sig_handler(int sig_num){
    /*
     * TODO:NOT IMPLEMENTED YET!
     * Function used to gracefully end interactive mode after SIGINT
     * was caught
     * Arguments:
     *      (int) sig_num - Ignored
     */
    END_INTERACTIVE = 1;
}

void set_root_server(){
    while(!CURRENT_SERVER->root){
        CURRENT_SERVER = CURRENT_SERVER->next;
    }
}

void summary(int option_flags){
    char time_unit[3] = "ms\0";
    float overhead = (float) ( (float)global_stat.total_bits_transmited / (float) ((float) global_stat.total_data_bits / (float) 100));
    float total_time_ms = (int) (global_stat.end_time - global_stat.start_time);

    if (total_time_ms > 1000){
        total_time_ms = (float) total_time_ms / 1000;
        strncpy(&time_unit, "s\0", 2);
    }

    printf("\n##############################\n");
    printf("### Transmission statistics:\n");
    printf("Total Transmited: %d b\n"
            "Pure data: %d b\n"
            "Transport ratio: %.2f %\n", global_stat.total_bits_transmited, global_stat.total_data_bits, overhead);

    if (((option_flags & READ_FLAG) == READ_FLAG) && ((option_flags & FEC_FLAG) == FEC_FLAG) ){
        printf("Errors fixed: %d\n",  global_stat.errors);

    }

    printf("Total time: %.2f %s",total_time_ms, time_unit);
    printf("\n##############################\n");

}

uint64_t build_header(){
    uint64_t header = S_DATA_LENGTH;
    uint16_t crc;
    uint8_t *p_len = (uint8_t *) &header;
    
    // Signal compression
    header <<= 1;
    if((GLOBAL_OPTIONS & COMPRESS_FLAG) == COMPRESS_FLAG)
        header |= 1;

    // Signal encryption
    header <<= 1;
    if ((GLOBAL_OPTIONS & ENCRYPTION_FLAG) == ENCRYPTION_FLAG)
        header |= 1;

    //Signal fwd. error correction
    header <<= 1;
    if((GLOBAL_OPTIONS & FEC_FLAG) == FEC_FLAG)
        header |= 1;

    crc = crc16(0, p_len, sizeof(uint32_t));
    header = header << 16;
    header |= crc;

    return header;

}

void compose_name(char *output, unsigned long seq) {
/*Function creates  domain name to be queried.
 *
 *Arguments:
 *	*output - pointer to memory where domain name will be stored
 *	 seq	- sequence number that gets appended to domain name (see NOTE)
 *
 *NOTE: This is very much a dummy function for now, lacking any complexity
 *
 * TODO: Merge with compose_sync_name() because data bits now use prefixes too
 */
    int plaintext_len = strlen(config->passphrase);
    int i;
    unsigned char hex_char[3];
    unsigned char *p_plaintext = calloc(sizeof(char), plaintext_len + 11);
    unsigned char hash_bin[_SHA_DIGEST_LEN];
    unsigned char charseq[11];
    sprintf(charseq, "%lu", seq);
    strncpy(p_plaintext, config->passphrase, plaintext_len);
    strcat(p_plaintext, charseq);
    plaintext_len = strlen(p_plaintext);
    sha1_block_sum(p_plaintext, plaintext_len, hash_bin);

    for(i = 0; i < _SHA_DIGEST_LEN; i++){
        sprintf(hex_char, "%x", hash_bin[i]);
        strcat(output, hex_char);
    }
    strcat(output, config->name_base);
}

void compose_sync_name(char *output, char *sync_target, unsigned long seq) {
    /*
     * Very Similar to compose_name() function but takes (char *)pointer to a sync_target
     * which is prepended before name.
     * Used to create domain names for synchronization bits.
     * Arguments:
     *      (char *) output         - char buffer to which a final name is composed
     *      (char *) sync_target    - prefix that is added before actual name
     *      (unsigned long) seq     - sequence number
     *
     */
    char str_index[32];
    sprintf(str_index, "%d", seq);
    strcpy(output, sync_target);
    strncat(output, str_index, sizeof(str_index));
    compose_name(output, 0);
}


int hamming74_bin_to_file(void **args) {
    int output_fd = (int)args[0];
    int input_fd = (int)args[1];
    int bit_index = 0;
    unsigned char hamming_buffer[14];
    unsigned char *p_bot_hamming_buffer = hamming_buffer;
    unsigned char *p_top_hamming_buffer = &hamming_buffer[7];
    unsigned char byte_buffer[8];
    unsigned char *p_bot_byte_buffer = byte_buffer;
    unsigned char *p_top_byte_buffer = &byte_buffer[4];
    ssize_t check = 0;
    char byte = 0;
    char bit;
    int i;
    int total_bits;

    while (REBUILD_INDEX < R_DATA_END) {
        check = read(input_fd, &bit, 1);
        if (check > 0) {
            REBUILD_INDEX++;
            if (strncmp(&bit, "1", 1) == 0) {
                hamming_buffer[bit_index] = 1;
            } else {
                hamming_buffer[bit_index] = 0;
            }
            if (bit_index == 13) {
                byte = 0;
                bit_index = -1;
                global_stat.errors += hamming74_decode_block(p_top_hamming_buffer, p_top_byte_buffer);
                global_stat.errors += hamming74_decode_block(p_bot_hamming_buffer, p_bot_byte_buffer);
                for(i=0; i < 8; i++ ){
                    byte = byte << 1;
                    byte |= byte_buffer[i];
                }
 
                write(output_fd, &byte, 1);
                total_bits += 8;
                //printf("Byte: %c\n",byte);
//                printf("%c\n",byte);
            }
            bit_index++;
        }
    }
    close(output_fd);
    return total_bits;
}
int bin_to_file(void **args) {
    /*
     * Function used to rebuild recieved bits back into bytes in correct order.
     * It is responsible for incrementing REBUILD_INDEX that is used by retriever_thread()
     * to determine which bit should be passed (via pipe) to this function.
     * Rebuilt bytes are written into output pipe.
     * Arguments:
     *      (void **) args  - pointer to array of two file descriptors that
     *                        represents input and output pipe
     *
     * TODO:Rework synchronization. If retriever_thread() fails to get 1 bit, whole process is blocked
     * TODO:Rework input argument to predefined structure
     */
    int output_fd = (int)args[0];
    int input_fd = (int)args[1];
    int bin_index = 7;
    int total_bits = 0;
    ssize_t check = 0;
    int byte = 0;
    char bit;

    while (REBUILD_INDEX < R_DATA_END) {
        check = read(input_fd, &bit, 1);
        if (check > 0) {
            REBUILD_INDEX++;
            total_bits ++;
            //printf("Bit: %c\n",bit);
            if (strncmp(&bit, "1", 1) == 0) {
                byte = (byte | (1 << bin_index));
                bin_index--;
            } else {
                bin_index--;
            }
            if (bin_index == -1) {
                write(output_fd, &byte, 1);
                //printf("Byte: %c\n",byte);
//                printf("%c\n",byte);
                bin_index = 7;
                byte = 0;
            }
        }
    }
    close(output_fd);
    return total_bits;
}

void sender_thread(int *fd) {
    /*
     * Function that is intended to run as separate thread. It reads data from pipe (specified by *fd) 1 byte
     * at a time. Recieved bytes are expected to be chars (either "0" or "1")representing bits of data to be sent.
     * If "1" is recieved, this bit is cached to a DNS server in CURRENT_SERVER with sequence number of S_BIT_INDEX.
     * Every received bit inceases S_DATA_LENGTH counter which represents length of whole message. At the end, value of
     * S_DATA_LENGTH is written into first 32bits of message.
     *
     * Arguments:
     *      (int *) fd  - pointer to file descriptor that is reading end of pipe
     */
    const struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 1000};
    struct timespec rem;
    char c;
    char *prefix;
    size_t prefix_len;
    uint64_t bitmask;
    ssize_t check;
    struct query_t query;
    query.domain_name = calloc(255, sizeof(char));
    if(!prefix_config.buddy_data_prefix){
        prefix = malloc(1);
        strncpy(prefix,"\0",1);
        prefix_len = 1;
    }else{
        prefix = prefix_config.buddy_data_prefix;
        prefix_len = strlen(prefix) + 1;
    }

    while (!END_WORKERS) {
        if (pthread_mutex_trylock(&SENDER_LOCK) == 0) {

            if(END_WORKERS){
                pthread_mutex_unlock(&SENDER_LOCK);
                break;
            }

            check = read(*fd, &c, 1);
            if (check > 0) {
                if (strncmp(&c, "1", 1) == 0) {
                    strncpy(query.domain_name, prefix, prefix_len);
                    compose_name(query.domain_name, S_BIT_INDEX);
                    query.name_server = CURRENT_SERVER->server;
                    S_BIT_INDEX++;
                    S_DATA_LENGTH++;
                    CURRENT_SERVER = CURRENT_SERVER->next;
                    pthread_mutex_unlock(&SENDER_LOCK);
                    exec_query(&query);
                } else {
                    S_BIT_INDEX++;
                    S_DATA_LENGTH++;
                    CURRENT_SERVER = CURRENT_SERVER->next;
                    pthread_mutex_unlock(&SENDER_LOCK);
                }
                global_stat.total_bits_transmited ++;
            } else if (check == -1 && errno == EAGAIN) {
                pthread_mutex_unlock(&SENDER_LOCK);
            } else {
                CURRENT_SERVER = config->name_server;
                S_HEADER = build_header();
                END_WORKERS = 1;
                pthread_mutex_unlock(&SENDER_LOCK);
            }
        }
        //nanosleep(&sleep_time, &rem);
    }
    // Write message length
    while (S_HEADER_INDEX <= S_HEADER_INDEX_STOP){
//        printf("writing header %d to %s \n", S_DATA_LENGTH, CURRENT_SERVER->server);
        if ((pthread_mutex_trylock(&SENDER_LOCK)) == 0){
            set_root_server();
            bitmask = (uint64_t)1 << S_HEADER_INDEX_REL;
            if(bitmask == (bitmask & S_HEADER)){
                strncpy(query.domain_name, prefix, prefix_len);
                compose_name(query.domain_name, S_HEADER_INDEX);
                //printf("Writing header %d %s\n",S_HEADER_INDEX, query.domain_name);
                query.name_server = CURRENT_SERVER->server;
                S_HEADER_INDEX++;
                S_HEADER_INDEX_REL++;
//                CURRENT_SERVER = CURRENT_SERVER->next;
                pthread_mutex_unlock(&SENDER_LOCK);
                exec_query(&query);
            }else{
                S_HEADER_INDEX++;
                S_HEADER_INDEX_REL++;
//                CURRENT_SERVER = CURRENT_SERVER->next;
                pthread_mutex_unlock(&SENDER_LOCK);
            }
        }
        //nanosleep(&sleep_time, &rem);
    }
    free(query.domain_name);
    return;
}


void join_threads(pthread_t *threads) {
    /*
     * Function that joins array of 10 worker threads pointed to by *threads. This is used to join threads created by
     * create_senders() and create_retrievers().
     * Arguments:
     *      (pthread_t *) threads   - pointer to array of 10 pthread_t
     */
    int i, check;
    for (i = 0; i < 5; i++) {
        //printf("WAITING FOR thread %u\n",threads[i]);
        check = pthread_join(threads[i], NULL);
        //printf("JOINED thread %u with status %d\n",threads[i], check);
    }
    free(threads);

}

pthread_t *create_senders(int *fd) {
    /*
     * Function creates 10 threads executing sender_thread() function.
     * Arguments:
     *      (int*) fd   - pointer to file descriptor that is passed to sender_thread()
     * Return:
     *      Function returns pointer to array of 10 pthread_t
     */
    int i;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_t *threads = malloc(sizeof(pthread_t) * 5);

    for (i = 0; i < 5; i++) {
        pthread_create(&threads[i], &attr, (void *) sender_thread, fd);
        //printf("Original thread %u\n",threads[i]);

    }

    return (threads);
}


void retriever_thread(int *fd) {
    /*
     * Function that is intended to run as separate thread. It receives bits with sequence numbers of R_BIT_INDEX from
     * DNS server in CURRENT_SERVER. After bit is received, function waits until REBUILD_INDEX matches sequence number
     * of recieved bit. When they match, 1 byte ("0" or "1") representing bit is written into pipe pointed to by *fd.
     * Method used to determine wether bit was in cache or not is pointe to by config->method()
     *
     * Arguments:
     *      (int*) fd   - pointer to a file descriptor (writing end of pipe)
     */
    const struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 1000};
    struct timespec rem;
    int result;
    char c;
    char *prefix;
    size_t prefix_len;
    unsigned long my_bit;
    struct query_t query;
    query.domain_name = calloc(255,sizeof(char));

    if(!prefix_config.my_data_prefix){
        prefix = malloc(1);
        strncpy(prefix,"\0",1);
        prefix_len = 1;
    }else{
        prefix = prefix_config.my_data_prefix;
        prefix_len = strlen(prefix) + 1;
    }

    //printf("[%u] Created\n",pthread_self());
    while (R_BIT_INDEX < R_DATA_END) {
        if (pthread_mutex_trylock(&RETRIEVER_LOCK) == 0) {

            if(R_BIT_INDEX >= R_DATA_END){
                pthread_mutex_unlock(&RETRIEVER_LOCK);
                break;}

            //nanosleep(&sleep_time, &rem);
            my_bit = R_BIT_INDEX;
            query.name_server = CURRENT_SERVER->server;
            R_BIT_INDEX++;
            CURRENT_SERVER = CURRENT_SERVER->next;
            pthread_mutex_unlock(&RETRIEVER_LOCK);
            strncpy(query.domain_name, prefix, prefix_len);
            compose_name(query.domain_name, my_bit);
            // printf("[%u] name: %s\n",pthread_self(), query.domain_name);
            // printf("[%u] server: %s\n",pthread_self(), query.name_server);

            result = config->method(&query);
            //printf("[%u] name: %s Result: %d\n",pthread_self(), query.domain_name, result);
            sprintf(&c, "%d", result);
            while (1) {
                if (REBUILD_INDEX == my_bit) {
                    write(*fd, &c, 1);
                    global_stat.total_bits_transmited ++;
                    break;
                }
                if (END_WORKERS) { break; }
            }
        }
    }
    free(query.domain_name);
    return;

}

pthread_t *create_retrievers(int *fd) {
    /*
     * Function creates 10 threads executing retriever_thread() function.
     * Arguments:
     *      (int*) fd   - pointer to file descriptor that is passed to retriever_thread()
     * Return:
     *      Function returns pointer to array of 10 pthread_t
     */
    int i;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_t *threads = malloc(sizeof(pthread_t) * 5);

    for (i = 0; i < 5; i++) {
        pthread_create(&threads[i], &attr, (void *)retriever_thread, fd);
        //printf("Original thread %u\n",threads[i]);

    }

    return (threads);
}


int stream_to_bits(void **args) {
    /*
     * Function used to convert bytes from input_pipe into bits and write their representation (either "0" or "1")
     * int output_pipe. On the other end of output_pipe there is sender_thread() function that takse care of writing
     * these bits into cache of DNS server.
     * Arguments:
     *      (void**) args   - pointer to array of 2 file descriptors representing input_pipe (arg[0]) and output pipe
     *                        (arg[1])
     * TODO:Rework input argument to predefined structure
     */
    int input_pipe = (int) args[0];
    int output_pipe = (int) args[1];
    ssize_t check = 0;
    int i = 0, mask;
    int total_read = 0;
    char byte;
    while (1) {
        check = read(input_pipe, &byte, 1);
        if (check == -1 && errno == EAGAIN) { continue; }
        else if (check < 1) { break; }
        mask = 128;
//        printf("%c\n",byte);
        for (i = 0; i < 8; i++) {
            if (byte & mask) {
                write(output_pipe, "1", 1);
                //printf("1");
            } else {
                write(output_pipe, "0", 1);
                //printf("0");
            }
            total_read ++;
            mask = mask >> 1;
        }
        //printf("\n");
    }
    close(output_pipe);
    //printf("CLOSING\n");
    return total_read;
}

int iscached_iter(struct query_t *query) {
    /*
     * This function executes non-recursive DNS query to determine whether bit was previously cached or not.
     * Arguments:
     *      (query_t*) query    - structure that holds information about query (domain_name, name_server)
     *
     * NOTE: this function server as wrapper for sake of naming consistency.
     */
    return (exec_query_no_recurse(query));
}

int iscached_time(struct query_t *query) {
    /* This function executes DNS query and employs Weighted k-NN algorithm
     * (see http://en.wikipedia.org/wiki/K-nearest_neighbors_algorithm#k-NN_regression)
     * to determine whether domain name was previously cached or not.
     *
     * Arguments:
     *      (query_t*) query    - structure that holds information about query (domain_name, name_server)
     */

    int delay = exec_query(query);
    //printf("[%u] delay: %d\n", pthread_self(), delay);
    double distance = 0;
    float cached_score = 0;
    float uncached_score = 0;
    int i;

    for (i = 0; i < config->precision; i++) {
        if (delay == sample_times->cached_set[i]) { continue; }
        distance = 1.0 / (delay - sample_times->cached_set[i]);
        cached_score += absolute_value(distance);

        if (delay == sample_times->uncached_set[i]) { continue; }
        distance = 1.0 / (delay - sample_times->uncached_set[i]);
        uncached_score += absolute_value(distance);
    }
    if (cached_score > uncached_score) {
        return (1);
    } else {
        return (0);
    }

}

int iscached_ttl(struct query_t *query) {
    int ttl;
    ttl = exec_query_ttl(query);
    int result;
    if (ttl < ttl_reference){
        result = 1;
    }else{
        result = 0;
    }
    return result;
}


void *set_member(void *value, size_t size) {
/*Function returns pointer to memory with desired value stored.
 * This pointer can be then assigned to structure member.
 *
 *Arguments:
 *	*value	- data to be stored
 *	size	- size of data &value
 *
 */
    void *p = calloc(size + 1, sizeof(char));
    strncpy(p, value, size);
    return p;
}

conf *init_conf() {
/*Function initialize and returns config structure
 *
 *XXX:Shouldnt be that path configurable? o.O
 */
    conf *c = malloc(sizeof(conf));
    c->conf_file = set_member("/etc/dns-cc/dns.cfg", sizeof("/etc/dns-cc/dns.cfg"));
    return c;
}

void free_servers(struct server_list *root) {
    /*
     * Function frees alle elements of circular linked list formed by struct server_list
     * Arguments:
     *      (struct server_list*) root  - pointer to any element in circular linked list
     */
    struct server_list *n = root->next;
    if (root != root->next) {
        while (1) {
            free(n->server);
            if (n->next == root) { break; }
            free(n);
            n = n->next;
        }
    }
    free(root->server);
    free(root);

}

void free_conf(struct conf *p) {
/*Functions frees config structure and all its members
 *
 */
    free(p->conf_file);
    free(p->input_file);
    free(p->output_file);
    free(p->name_base);
    free_servers(p->name_server);
    free(p->passphrase);
    free(p);
    return;
}

void help() {
/*This Function will display help and usage information
 *XXX:Replace dummy text with actual help
 */
    printf("\nOne day, this will be nice help :)\n");
}

void remove_blank(char *str) {
/*Function removes all blank and control characters from string except final \0
 *(Used to preparse lines from config file)
 *
 * Arguments:
 *	*str	- pointer to string to be cleaned
 */
    char *clean_str = calloc(strlen(str), sizeof(char));
    char *buf = malloc(1);
    int i = 0;
    while (1) {
        memcpy(buf, &str[i], 1);
        if (strcmp(buf, "\0") == 0) { break; }
        if (iscntrl(buf[0]) != 0) {
            ++i;
            continue;
        }
        if (isblank(buf[0]) == 0) { strncat(clean_str, buf, 1); }
        ++i;
    }
    strcat(clean_str, "\0");
    strcpy(str, clean_str);
    free(clean_str);
    free(buf);
    return;
}

void test() {
    /*
     * Used for testing purposes only!
     */
   
    unsigned char c[3];
    char name_server[20] = "192.168.50.11";
    char domain_name[25] = "asd24.diplo.anoobis.sk";
    char *p_domain = domain_name;
    int d_len = strlen(&domain_name);
    unsigned char hex_sum[(_SHA_DIGEST_LEN * 2)];
    unsigned char *p_hex_sum = hex_sum;
    unsigned char sum[_SHA_DIGEST_LEN];
    unsigned char *p_sum = sum;
    compose_name(p_hex_sum, 5);
    printf("%s\n",p_hex_sum);
    return;
    int i;
    sha1_block_sum(p_domain, d_len, p_sum);
    printf("Result:\n");
    for (i = 0; i < _SHA_DIGEST_LEN; i++){
        sprintf(c, "%x", p_sum[i]);
        strcat(hex_sum, c);
    }
    printf("%s\n",hex_sum);
    //exec_query(&query);
}

void set_servers(char *servers) {
    /*
     * Function used to create circular linked list of struct server_list
     * Arguments:
     *      (char*) servers - pointer to comma separated string containing IP addresses of DNS servers
     *
     * TODO:Perform IP address sanity check
     */
    char *token;
    struct server_list *prev, *new, *first;

    token = strsep(&servers, ",");
    first = calloc(1, sizeof(struct server_list));
    first->server = calloc(1, strlen(token));
    strncpy(first->server, token, strlen(token));
    first->root = TRUE;
    first->next = first;
    config->name_server = first;
    prev = first;

    while ((token = strsep(&servers, ","))) {
        //XXX:Valgrind says I'm loosing memory here
        new = calloc(1, sizeof(server_list));
        new->server = calloc(1, strlen(token));
        strncpy(new->server, token, strlen(token));
        new->root = FALSE;
        new->next = first;
        prev->next = new;
        prev = new;
    }
}

void set_conf(char *var, char *value) {
/*Function sets config structure members (if var coresponds to struct member)
 *
 *Arguments
 *	var		- struct member to be set
 *	value	- data for struct member
 */
    size_t value_length = strlen(value);
    if (strcasecmp(var, "server") == 0) {
        set_servers(value);
        CURRENT_SERVER = config->name_server;
        return;
    }
    if (strcasecmp(var, "base_name") == 0) {
        config->name_base = set_member(value, value_length);
        return;
    }
    if (strcasecmp(var, "passphrase") == 0) {
        config->passphrase = set_member(value, value_length);
        return;
    }
    if (strcasecmp(var, "precision") == 0) {
        config->precision = atoi(value);
        return;
    }
    if (strcasecmp(var, "max_speed") == 0) {
        config->max_speed = atoi(value);
        return;
    }


}

void read_conf_file() {
/*Function parses config file and sets config structure members
 *
 */
    FILE *fp;
    fp = fopen(config->conf_file, "r");
    size_t line_length = 200;
    char *var;
    char *line = malloc(line_length);

    if (fp == NULL) {
        printf("Can't open configuration file. %s\n", config->conf_file);
        exit(0);
    }

    printf("Using config file %s\n", config->conf_file);

    while ((getline(&line, &line_length, fp)) != -1) {
        if (strncmp(line, "#", 1) == 0 || strncmp(line, "\n", 1) == 0) { continue; }
        line_length = (int) strlen(line);

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

sample *init_sample_times() {
/*Function creates struct and alocates space (based on config->precision)
 * for sample time values used by iscached() to determine whether domain
 * name was cached or not
 */
    sample *s = malloc(sizeof(sample));
    s->cached_set = calloc(config->precision, sizeof(int));
    s->uncached_set = calloc(config->precision, sizeof(int));
    return s;
}

void free_sample_times() {
/*Function frees structure holding sample times and its members
 *
 */
    free(sample_times->cached_set);
    free(sample_times->uncached_set);
    free(sample_times);
}

void read_header() {
    /*
     * Function reads first 4 bytes of message starting at sequence number of R_HEADER_INDEX to determine overall
     * length of message (in bits) whch is then set to R_DATA_LENGTH
     *
     * NOTE: Final length includes also first 4 bytes used as length header.
     */
    struct query_t query;
    query.domain_name = calloc(255, sizeof(char));
    uint64_t result;
    int index_stop = R_HEADER_INDEX + 64;
    int relative_index = 0;
    char *prefix;
    size_t prefix_len;
    uint64_t header = 0;
    uint16_t crc;
    uint8_t flag;

    if(!prefix_config.my_data_prefix){
        prefix = malloc(1);
        strncpy(prefix,"\0",1);
        prefix_len = 1;
    }else{
        prefix = prefix_config.my_data_prefix;
        prefix_len = strlen(prefix) + 1;
    }

    R_DATA_LENGTH = 0;
    set_root_server();
    for (R_HEADER_INDEX; R_HEADER_INDEX < index_stop; R_HEADER_INDEX++) {
        strncpy(query.domain_name, prefix, prefix_len);
        compose_name(query.domain_name, R_HEADER_INDEX);
        query.name_server = CURRENT_SERVER->server;
        result = config->method(&query);
        //printf("Position %s, result %d\n",query.domain_name, result);
        header = (header | (result << relative_index));
        relative_index++;
    }
    // TODO: Check checksum
    // Extract header crc
    crc = header & 0xFFFF;
    header = header >> 16;
    
    //Extract fwd. error correction flag
    flag = header & 0x1;
    header >>= 1;
    if (flag)
        GLOBAL_OPTIONS += FEC_FLAG;

    //Extract encryption flag
    flag = header & 0x1;
    header >>= 1;
    if (flag)
        GLOBAL_OPTIONS += ENCRYPTION_FLAG;

    //Extract compression flag
    flag = header & 0x1;
    header >>= 1;
    if (flag)
        GLOBAL_OPTIONS += COMPRESS_FLAG;

    R_DATA_LENGTH = header;

    //TODO: NOtify user about flag discrepancies
    //printf("d_len %u \n crc %04x\n flags %d\n", R_DATA_LENGTH, crc, GLOBAL_OPTIONS);
    R_DATA_LENGTH += 64;
    R_DATA_END += R_DATA_LENGTH;
    CURRENT_SERVER = config->name_server;
    free(query.domain_name);
}

void calibrate() {
/*Function colects sample times for cached and not cached domain names.
 *Number of samples is based on config->precision defined by user
 *
 */
//    TODO: Create per-server calibration sets instead of globals.
    srand(time(NULL));
    unsigned int i = 0;
    struct server_list *servers = config->name_server;
    struct query_t query;
    query.domain_name = calloc(255, sizeof(char));
    
    if (config->method == iscached_time){
        sample_times = init_sample_times();
        for (i = 0; i < config->precision; i++) {
            query.name_server = servers->server;
            strcpy(query.domain_name, "precise.\0");
            compose_name(query.domain_name, (unsigned long) rand());
            sample_times->uncached_set[i] = exec_query(&query);
            sample_times->cached_set[i] = exec_query(&query);
            servers = servers->next;
        }
    }
    else if(config->method == iscached_iter){
        // No need to calibrate here
    }
    else if(config->method == iscached_ttl){
        query.name_server = servers->server;
        strcpy(query.domain_name, "precise.\0");
        compose_name(query.domain_name, (unsigned long) rand());
 
        ttl_reference =  exec_query_ttl(&query);
        //printf("TTL ref: %d\n",ttl_reference);
    }

/*	printf("Cached:");
	for(i = 0; i < config->precision; i++){
	printf(" %d,",sample_times->cached_set[i]);
	}
	printf("\n");
	
	printf("uncached:");
	for(i = 0; i < config->precision; i++){
	printf(" %d,",sample_times->uncached_set[i]);
	}
	printf("\n");
*/

    free(query.domain_name);
    return;
}

void get_sync_target(){
    /*
     * Function used in interactive mode to determine users position in conversation (either user1 or user2,
     * more users are not supported). If both conversation slots are full,program ends.
     * After successfully finding slot, prefix_config structure is filled with proper values for two way interactive
     * conversation.
     */
    struct query_t query_first;
    struct query_t query_second;
    query_first.domain_name = calloc(255, sizeof(char));
    query_first.name_server = config->name_server->server;
    compose_sync_name(query_first.domain_name, &USER1_SYNC, BUDDY_SYNC_INDEX);

    query_second.domain_name = calloc(255, sizeof(char));
    query_second.name_server = config->name_server->server;
    compose_sync_name(query_second.domain_name, &USER2_SYNC, BUDDY_SYNC_INDEX);

    if ((config->method(&query_first)) == 0){
        exec_query(&query_first);
        prefix_config.my_sync = &USER1_SYNC;
        prefix_config.buddy_sync = &USER2_SYNC;
        prefix_config.my_data_prefix = &USER1_PREFIX;
        prefix_config.buddy_data_prefix = &USER2_PREFIX;
        printf("I'm first\n");
    }else if ((config->method(&query_second)) == 0){
        exec_query(&query_second);
        prefix_config.my_sync = &USER2_SYNC;
        prefix_config.buddy_sync = &USER1_SYNC;
        prefix_config.my_data_prefix = &USER2_PREFIX;
        prefix_config.buddy_data_prefix = &USER1_PREFIX;
        printf("I'm second\n");
    }else{
        printf("Conversation full\n");
        exit(EXIT_FAILURE);
    }
    MY_SYNC_INDEX++;
    return;
}

void set_sync_bit(){
    /*
     * Function used in interactive mode to announce other conversation articipant that message theres new message.
     * TODO:Implement sleep to ensure max bitrate
     */
    int result = 1;
    char *sync_name = calloc(255, sizeof(char));
    struct query_t query;
    while (result == 1){
        compose_sync_name(sync_name, prefix_config.buddy_sync, BUDDY_SYNC_INDEX);
        query.domain_name = sync_name;
        query.name_server = config->name_server->server;
        result = config->method(&query);
        BUDDY_SYNC_INDEX++;
    }
//    printf("*Message sent*\n");
    set_status(&STAT_SENT_OK);
    exec_query(&query);
    return;
}

void wait_for_sync_bit(){
    /*
     * Function used in interactive mode to wait until next message is ready to be read.
     */
    int result = 0;
    char *sync_name = calloc(255, sizeof(char));
    struct query_t query;
    while(result == 0){
        compose_sync_name(sync_name, prefix_config.my_sync, MY_SYNC_INDEX);
        query.domain_name = sync_name;
        query.name_server = config->name_server->server;
        result = config->method(&query);
        MY_SYNC_INDEX++;
        exec_query(&query);
        //printf("\nName: %s, Result: %d\n",query.domain_name, result);
        //TODO:Make sleep period configurable!
        sleep(5);
    }
    return;
}

void interactive_sender(){
    /*
     * Function intended to be run in separate thread. Used in interactive mode, this function reads contents of stdin.
     * until it reaches EOF. Data from stdin are passed to stream_to_bits() function via pipe.
     */
    struct sigaction sigint_action_struct = {.sa_handler = &sig_handler };
    char c;
    char *buffer = calloc(1000, sizeof(char));
    pthread_t *workers;
    pthread_t stream = {0};

    /*TODO:Gracefull handling of SIGINT
    sigaction(SIGINT,&sigint_action_struct, NULL);
    */

    int bit_pipe[2];
    int display_pipe[2];

    while (END_INTERACTIVE == 0) {
        pipe(bit_pipe);
        pipe(display_pipe);
        workers = create_senders(&bit_pipe[0]);
        void *stream_args[2] = {display_pipe[0], bit_pipe[1]};
        pthread_create(&stream, NULL, (void *) stream_to_bits, stream_args);
        connect_display_input(display_pipe[1]);
        pthread_join(stream, NULL);
        close(bit_pipe[0]);
        close(bit_pipe[1]);
        join_threads(workers);
        set_sync_bit();

        S_HEADER_INDEX_REL = 0;
        S_HEADER_INDEX = S_BIT_INDEX;
        S_HEADER_INDEX_STOP = S_HEADER_INDEX + 31;
        S_BIT_INDEX = S_HEADER_INDEX + 32;
        S_DATA_LENGTH = 0;
        END_WORKERS = 0;
    }
    printf("Exiting\n");
    return;
}

void interactive_listener(){
    /*
     * Function intended to be run in separate thread. Used in interactive mode, this function waits until
     * synchronization bit is set. When synchronization bit signals new message, this message is printed to stdout.
     */

    pthread_t *workers;
    pthread_t stream = {0};
    int bit_pipe[2];
    int display_pipe[2];

    while (END_INTERACTIVE == 0) {
        pipe(bit_pipe);
        pipe(display_pipe);
        wait_for_sync_bit();
        read_header();
//        printf("Data length: %d\n",R_DATA_LENGTH);
//        printf("\n*Recieving message*\n");
        set_status(&STAT_RECIEVING);
        void *stream_args[2] = {display_pipe[1], bit_pipe[0]};
        pthread_create(&stream,NULL,(void *) bin_to_file, (void *) stream_args);
        workers = create_retrievers(&bit_pipe[1]);
        connect_display_output(display_pipe[0]);
        pthread_join(stream, NULL);
        join_threads(workers);
        close(bit_pipe[0]);
        close(bit_pipe[1]);

        R_HEADER_INDEX = R_BIT_INDEX;
        R_BIT_INDEX = R_BIT_INDEX + 32;
        REBUILD_INDEX = R_BIT_INDEX;
        set_status(&STAT_IDLE);
//        printf("Type in your message (Enter + ^D to finish): ");
        fflush(stdout);
    }
    return;
}

int compresor(void **args) {
/*Function calls deflate_data() from compres.h with appropriate arguments.
 *It is meant to be us in thread creation
 *
 * Arguments:
 *	arg[0]	- FILE pointer to opened input file
 *	arg[1]	- File descriptor to write end of pipe
 */
    FILE *fp = (FILE *) args[1];
    int fd = (int) args[0];
    int total_bits;
    total_bits = deflate_data(fp, fd);
    close(fd);
    return total_bits;
}

int encryptor(void **args) {
/*Function calls encrypt_stream() from crypto.h with appropriate arguments.
 *It is meant to be us in thread creation
 *
 * Arguments:
 *	arg[0]	- FILE pointer to opened input file
 *	arg[1]	- File descriptor to write end of pipe
 */
    int fd_in = fileno((FILE *) args[1]);
    int fd_out = (int) args[0];
    int input_bits;
    input_bits = encrypt_stream(fd_in, fd_out, config->passphrase);
    close(fd_out);
    return input_bits;
}

int decryptor(void **args) {
/*Function calls decrypt_stream() from crypto.h with appropriate arguments.
 *It is meant to be us in thread creation
 *
 * Arguments:
 *	arg[0]	- FILE pointer to opened input file
 *	arg[1]	- File descriptor to write end of pipe
 */
    int fd_in = (int) args[1];
    int fd_out = (int) args[0];
    int total_bits;
    total_bits = decrypt_stream(fd_in, fd_out, config->passphrase);
    //close(fd_out);
    return total_bits;
}

int reading_mode(){
    FILE *output_fp = fopen(config->output_file, "wb");
    int output_fd = fileno(output_fp);
    printf("Retrieving data...\n");
    calibrate();
    read_header();

    int binary_pipe[2];
    pipe(binary_pipe);

    int data_pipe[2];
    pipe(data_pipe);
    pthread_t rebuild_td = {0};
    pthread_t decompres_td = {0};

    // Start time measurment
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    global_stat.start_time = (int) ((tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec));


    if ((GLOBAL_OPTIONS & COMPRESS_FLAG) == COMPRESS_FLAG) {
        void *decompres_args[2] = {output_fd, data_pipe[0]};
        pthread_create(&decompres_td, NULL, (int) inflate_data, (void *) decompres_args);

        void *args[2] = {data_pipe[1], binary_pipe[0]};
        pthread_create(&rebuild_td, NULL, (void *) bin_to_file, (void *) args);

        pthread_t *workers_td = create_retrievers(&binary_pipe[1]);

        pthread_join(rebuild_td, NULL);
        pthread_join(decompres_td, &global_stat.total_data_bits);
        join_threads(workers_td);
    }

    else if ((GLOBAL_OPTIONS & ENCRYPTION_FLAG) == ENCRYPTION_FLAG) {
        void *args[2] = {data_pipe[1], binary_pipe[0]};
        pthread_create(&rebuild_td, NULL, (int) bin_to_file, (void *) args);

        pthread_t *workers_td = create_retrievers(&binary_pipe[1]);

        void *decrypt_args[2] = {output_fd, data_pipe[0]};
        pthread_create(&decompres_td, NULL, (void *) decryptor, (void *) decrypt_args);

 

        pthread_join(rebuild_td, NULL);
        pthread_join(decompres_td, &global_stat.total_data_bits);
        join_threads(workers_td);
 
    } else if ((GLOBAL_OPTIONS & FEC_FLAG) == FEC_FLAG){
        int *args[2] = {output_fd, binary_pipe[0]};
        pthread_create(&rebuild_td, NULL, (int) hamming74_bin_to_file, (int *) args);

        pthread_t *workers_td = create_retrievers(&binary_pipe[1]);

        pthread_join(rebuild_td, &global_stat.total_data_bits);
        join_threads(workers_td);
    } else {
        int *args[2] = {output_fd, binary_pipe[0]};
        pthread_create(&rebuild_td, NULL, (int) bin_to_file, (void *) args);

        pthread_t *workers_td = create_retrievers(&binary_pipe[1]);

        pthread_join(rebuild_td, &global_stat.total_data_bits);
        join_threads(workers_td);
    }

    // End time measurment
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    global_stat.end_time = (int) ((tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec));

    printf("Done Reading\n");
    close(output_fd);
    fclose(output_fp);
    //free_sample_times();
    free_conf(config);
    return(EXIT_SUCCESS);
}

int sending_mode(){
    FILE *fp;
    if (strcmp(config->input_file, "-") == 0) {
//            printf("Type in your message (end with ^D):  ");
        fp = stdin;
    } else {
        fp = fopen(config->input_file, "r");
    }

    if (fp == NULL) {
        printf("Can't open input file: %s\n", config->input_file);
        return(EXIT_FAILURE);
    }
    printf("Sending Data...\n");
    int input_fd = fileno(fp);
    int *byte = calloc(1, sizeof(int));
    int data_pipe[2];
    int binary_pipe[2];

    pipe(data_pipe);
    fcntl(data_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(data_pipe[1], F_SETFL, O_NONBLOCK);

    pipe(binary_pipe);
    fcntl(data_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(data_pipe[1], F_SETFL, O_NONBLOCK);


    pthread_t filestream_td = {0};
    pthread_t bitstream_td = {0};

    pthread_t *workers_td;
//    printf("creating threads\n");
    workers_td = create_senders(&binary_pipe[0]);
    
    // Start time measurment
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    global_stat.start_time = (int) ((tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec));

    if ((GLOBAL_OPTIONS & COMPRESS_FLAG) == COMPRESS_FLAG) {
        void *compres_args[2] = {data_pipe[1], fp};
        pthread_create(&filestream_td, NULL, (int) compresor, (void *) compres_args);

        void *stream_args[2] = {data_pipe[0], binary_pipe[1]};
        pthread_create(&bitstream_td, NULL, (void *) stream_to_bits, (void *) stream_args);

        pthread_join(filestream_td, &global_stat.total_data_bits);
        pthread_join(bitstream_td, NULL);
        close(data_pipe[0]);
    } else if ((GLOBAL_OPTIONS & ENCRYPTION_FLAG) == ENCRYPTION_FLAG) {
        void *encrypt_args[2] = {data_pipe[1], fp};
        pthread_create(&filestream_td, NULL, (int) encryptor, (void *) encrypt_args);

        void *stream_args[2] = {data_pipe[0], binary_pipe[1]};
        pthread_create(&bitstream_td, NULL, (int) stream_to_bits, (void *) stream_args);

        pthread_join(filestream_td, &global_stat.total_data_bits);
        pthread_join(bitstream_td, NULL);
        close(data_pipe[0]);
    }else if ((GLOBAL_OPTIONS & FEC_FLAG) == FEC_FLAG){
        int *stream_args[2] = {input_fd, binary_pipe[1]};
        pthread_create(&bitstream_td, NULL, (int) hamming74_encode_stream, (int *) stream_args);

//        pthread_join(filestream_td, NULL);
        pthread_join(bitstream_td, &global_stat.total_data_bits);
        close(data_pipe[0]);
       
    } else {
        void *stream_args[2] = {input_fd, binary_pipe[1]};
        pthread_create(&bitstream_td, NULL, (int) stream_to_bits, (void *) stream_args);

//        pthread_join(filestream_td, NULL);
        pthread_join(bitstream_td, &global_stat.total_data_bits);
        close(data_pipe[0]);
    }

    join_threads(workers_td);

    // End time measurment
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    global_stat.end_time = (int) ((tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec));


    fclose(fp);
    printf("Data sent successfuly\n");
    free(byte);
    free_conf(config);
    return(EXIT_SUCCESS);
}

int interactive_mode(){
    calibrate();
    get_sync_target();
    pthread_t sender = {0};
    pthread_t listener = {0};

    display_init();

    pthread_create(&sender, NULL, (void*) interactive_sender, NULL);
    pthread_create(&listener, NULL, (void*) interactive_listener, NULL);

    pthread_join(sender, NULL);
    printf("Joined sender\n");
    pthread_join(listener, NULL);
    display_destroy();
    return(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&SENDER_LOCK, &mutex_attr);
    int c, option_flags = 0, exit_status = EXIT_FAILURE;
    config = init_conf();
    while ((c = getopt(argc, argv, "vfiteDCm:c:s:r:")) != -1) {
        switch (c) {
            case 'i':
                //interactive mode
                if (config->method == NULL) {
                    //If method is not set, use default
                    config->method = &iscached_time;
                }
                GLOBAL_OPTIONS += INTERACTIVE_FLAG;
                break;
            case 't':
                //testing option
                read_conf_file();
                test();
                exit(EXIT_SUCCESS);
            case 'c':
                //Redefine config file
                free(config->conf_file);
                config->conf_file = set_member(optarg, strlen(optarg));
                break;
            case 'D':
            case 'C':
                //Are we gonna (de)compress?
                GLOBAL_OPTIONS += COMPRESS_FLAG;
                break;
            case 'e':
                //Encryption used
                GLOBAL_OPTIONS += ENCRYPTION_FLAG;
                break;
            case 'f':
                GLOBAL_OPTIONS += FEC_FLAG;
                break;
            case 's':
                //program is to send data. If source file is -, open stdin
                config->input_file = set_member(optarg, strlen(optarg));
                GLOBAL_OPTIONS += SEND_FLAG;
                config->output_file = set_member("\0", strlen("\0"));
                break;

            case 'r':
                //program is to recieve data from DNS server and write them into file.
                config->output_file = set_member(optarg, strlen(optarg));
                if (config->method == NULL) {
                    //If method is not set, use default
                    config->method = &iscached_time;
                }
                GLOBAL_OPTIONS += READ_FLAG;
                break;
            case 'm':
                //define method which is used to determine if bit was cached or no
                if (strcmp(optarg, "time") == 0) {
                    config->method = &iscached_time;
                } else if (strcmp(optarg, "iterative") == 0) {
                    config->method = &iscached_iter;
                } else if (strcmp(optarg, "ttl") == 0) {
                    config->method = &iscached_ttl;
                } else {
                    printf("ERROR: Unknown method\n");
                    help();
                    return (EXIT_FAILURE);
                }
                break;
            case 'v':
                VERBOSITY = 1;
                break;
            default:
                help();
                return (EXIT_FAILURE);
        }
    }
    if ((GLOBAL_OPTIONS & READ_FLAG) == READ_FLAG && (GLOBAL_OPTIONS & SEND_FLAG) == SEND_FLAG) {
        printf("You can't (-s)end and (-r)ecieve at same time, your options do not make sense\n");
        help();
        return (EXIT_FAILURE);
    }

    read_conf_file();

    if ((GLOBAL_OPTIONS & INTERACTIVE_FLAG) == INTERACTIVE_FLAG){
        exit_status = interactive_mode();
    }else if ((GLOBAL_OPTIONS & SEND_FLAG) == SEND_FLAG) {
        exit_status = sending_mode();
    }else if ((GLOBAL_OPTIONS & READ_FLAG) == READ_FLAG) {
        exit_status = reading_mode();
    }else{
        help();
    }
    /*printf("input_file %s\n", config->input_file);
    printf("name_server %s\n", config->name_server);
    printf("name_base %s\n", config->name_base);
    printf("key %s\n", config->key);*/
    //free_conf(config);
    if(VERBOSITY){
        summary(option_flags);
    }
    return (exit_status);
}
