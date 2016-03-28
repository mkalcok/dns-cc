//
// Created by kauchman on 15.3.2016.
//

#ifndef DNS_CC_NAMERES_H
#define DNS_CC_NAMERES_H

#include <ares.h>

typedef struct query_t{
    char *domain_name;
    char *name_server;
};

void gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host);

void time_query_cb(unsigned long *timer_slot, int status, int timeouts, unsigned char *abuf, int alen);
void norecurse_query_cb(int *response, int status, int timeouts, unsigned char *abuf, int alen);

void main_loop(ares_channel channel);

int exec_query(struct query_t *query);
int exec_query_no_recurse(struct query_t *query);
#endif //DNS_CC_NAMERES_H
