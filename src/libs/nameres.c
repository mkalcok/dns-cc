#import "nameres.h"
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__

#include <time.h>

#endif

#ifdef __APPLE__

#include <mach/mach.h>
#include <mach/clock.h>
#include <sys/select.h>

#endif

void gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host) {
    if (status == ARES_SUCCESS) {
        printf("%s\n", inet_ntoa(*((struct in_addr *) host->h_addr)));
        printf("%d\n", status);
    }
    else
        printf("lookup failed: %d\n", status);
}

void time_query_cb(unsigned long *timer_slot, int status, int timeouts, unsigned char *abuf, int alen) {
#ifdef __linux__
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
#endif

#ifdef __APPLE__
    mach_timespec_t tv;
    clock_serv_t cclock;
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &tv);
    mach_port_deallocate(mach_task_self(), cclock);
#endif

    *timer_slot = (unsigned long) ((tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec));
}

void norecurse_query_cb(int *response, int status, int timeouts, unsigned char *abuf, int alen) {
    if (status == ARES_ENODATA || status == ARES_ESERVFAIL) {
        *response = 0;
        //printf("ares status: %d\n",status);
    } else if (status == ARES_SUCCESS || status == ARES_ENOTFOUND) {
        *response = 1;
    } else {
        //TODO:Temporary solution if other error occures
        *response = 1;
    }

}

void ttl_query_cb(int *response, int status, int timeouts, unsigned char *abuf, int alen) {
    struct hostent *p_host = gethostent();
    struct hostent *pp_host = &p_host;
    struct ares_addrttl  ttls;
    int n_ttls=1;
    int res;
    if (status == ARES_ENODATA || status == ARES_ESERVFAIL) {
        res = ares_parse_a_reply(abuf, alen,  pp_host, &ttls, &n_ttls);
        *response = ttls.ttl;
    } else if (status == ARES_SUCCESS || status == ARES_ENOTFOUND) {
        res = ares_parse_a_reply(abuf, alen, pp_host, &ttls, &n_ttls);
        *response = ttls.ttl;
    } else {
        //TODO:Temporary solution if other error occures
        *response = -1;
        
    }

}


void main_loop(ares_channel channel) {
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

int exec_query(struct query_t *query) {
    unsigned long timer[2] = {0, 0};
    struct ares_options options;
//    struct timespec tv;
    int res, delay;
    struct in_addr *server_addr = malloc(sizeof(struct in_addr));
    inet_aton(query->name_server, server_addr);
    options.servers = server_addr;
    options.nservers = 1;
    ares_channel channel;
    if ((res = ares_init_options(&channel, &options, ARES_OPT_SERVERS)) != ARES_SUCCESS) {
        printf("ares failed: %d\n", res);
        return 1;
    }
    //ares_gethostbyname(channel, name, AF_INET, dns_callback, NULL);
    //main_loop(channel);
    //TODO:Make it atomic!!!!
    ares_query(channel, query->domain_name, 1, 1, (ares_callback) time_query_cb, &timer[1]);

#ifdef __linux__
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
#endif

#ifdef __APPLE__
    mach_timespec_t tv;
    clock_serv_t cclock;
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &tv);
    mach_port_deallocate(mach_task_self(), cclock);
#endif

    timer[0] = (unsigned long) ((tv.tv_sec * 1000) + (0.000001 * tv.tv_nsec));
    main_loop(channel);
    delay = (int) (timer[1] - timer[0]);
    ares_destroy(channel);
    free(server_addr);
    return delay;
}

int exec_query_no_recurse(struct query_t *query) {
    struct ares_options options;
    int res, response;
    struct in_addr *server_addr = malloc(sizeof(struct in_addr));
    inet_aton(query->name_server, server_addr);
    options.flags = (ARES_FLAG_NORECURSE | ARES_FLAG_NOCHECKRESP);
    options.servers = server_addr;
    options.nservers = 1;
    ares_channel channel;
    if ((res = ares_init_options(&channel, &options, (ARES_OPT_SERVERS | ARES_OPT_FLAGS))) != ARES_SUCCESS) {
        printf("ares failed: %d\n", res);
        return 1;
    }
    ares_query(channel, query->domain_name, 1, 1, (ares_callback) norecurse_query_cb, &response);
    main_loop(channel);
    ares_destroy(channel);
    free(server_addr);
    return response;
}

int exec_query_ttl(struct query_t *query) {
    struct ares_options options;
    int res, response;
    struct in_addr *server_addr = malloc(sizeof(struct in_addr));
    inet_aton(query->name_server, server_addr);
    options.servers = server_addr;
    options.nservers = 1;
    ares_channel channel;
    if ((res = ares_init_options(&channel, &options, (ARES_OPT_SERVERS | ARES_OPT_FLAGS))) != ARES_SUCCESS) {
        printf("ares failed: %d\n", res);
        return 1;
    }
    ares_query(channel, query->domain_name, 1, 1, (ares_callback) ttl_query_cb, &response);
    main_loop(channel);
    ares_destroy(channel);
    free(server_addr);
    return response;
}

