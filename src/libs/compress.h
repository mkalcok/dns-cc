//
// Created by kauchman on 15.3.2016.
//

#ifndef DNS_CC_COMPRESS_H
#define DNS_CC_COMPRESS_H

#include <stdio.h>

#define CHUNK 16384
#define LEVEL 9

int inflate_data(void **args);
int deflate_data(FILE *source_file, int dest_fd);
#endif //DNS_CC_COMPRESS_H
