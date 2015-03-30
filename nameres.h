#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ares.h>


typedef struct query_t{
	char *domain_name;
	char *name_server;
}query_t;

void gethostbyname_cb (void* arg, int status, int timeouts, struct hostent* host)
{
    if(status == ARES_SUCCESS){
		printf("%s\n",inet_ntoa(*((struct in_addr *)host->h_addr)));
		printf("%d\n",status);
	}
    else
        printf("lookup failed: %d\n",status);
}

void time_query_cb(unsigned long *timer_slot, int status, int timeouts, unsigned char *abuf, int alen ){
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
	*timer_slot = (tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec);
}

void norecurse_query_cb(int *response, int status, int timeouts, unsigned char *abuf, int alen ){
	if(status == ARES_ENODATA || status == ARES_ESERVFAIL){
		*response = 0;
		printf("ares status: %d\n",status);
	}else if(status == ARES_SUCCESS || status == ARES_ENOTFOUND){
		*response = 1;
	}else{
		//TODO:Temporary solution if other error occures
		*response = 1;
	}
	
}

void main_loop(ares_channel channel){
    int nfds, count;
    fd_set readers, writers;
    struct timeval tv, *tvp;
    while (1) {
        FD_ZERO(&readers);
        FD_ZERO(&writers);
        nfds = ares_fds(channel, &readers, &writers);
        if (nfds == 0)
          break;
        tvp = ares_timeout(channel, NULL, &tv);
        count = select(nfds, &readers, &writers, NULL, tvp);
        ares_process(channel, &readers, &writers);
     }
}

int exec_query(query_t *query){
	unsigned long timer[2] = {0,0};
    struct ares_options options;
	struct timespec tv;
    int res = 500, delay;
	struct in_addr *server_addr = malloc(sizeof(struct in_addr));
    inet_aton(query->name_server, server_addr);
	options.servers = server_addr;
	options.nservers = 1;
    ares_channel channel;
    if((res = ares_init_options(&channel, &options, ARES_OPT_SERVERS)) != ARES_SUCCESS) {
        printf("ares failed: %d\n", res);
        return 1;
    }
    //ares_gethostbyname(channel, name, AF_INET, dns_callback, NULL);
    //main_loop(channel);
    //TODO:Make it atomic!!!!
    ares_query(channel, query->domain_name, 1 ,1, time_query_cb, &timer[1]);
	clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
	timer[0] = (tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec);
    main_loop(channel);
	delay = timer[1] - timer[0];
	ares_destroy(channel);
	free(server_addr);
    return delay;
  }

int exec_query_no_recurse(query_t *query){
    struct ares_options options;
    int res, response;
	struct in_addr *server_addr = malloc(sizeof(struct in_addr));
    inet_aton(query->name_server, server_addr);
	options.flags = (ARES_FLAG_NORECURSE | ARES_FLAG_NOCHECKRESP);
	options.servers = server_addr;
	options.nservers = 1;
    ares_channel channel;
    if((res = ares_init_options(&channel, &options, (ARES_OPT_SERVERS | ARES_OPT_FLAGS))) != ARES_SUCCESS) {
        printf("ares failed: %d\n", res);
        return 1;
    }
    ares_query(channel, query->domain_name, 1 ,1, norecurse_query_cb, &response);
    main_loop(channel);
	ares_destroy(channel);
	free(server_addr);
    return response;
  }
