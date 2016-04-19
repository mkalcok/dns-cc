//
// Created by Martin Kalcok on 12/04/16.
//

#ifndef DNS_CC_FEC_H
#define DNS_CC_FEC_H

void hamming74_decode_stream(int **args);
int hamming74_encode_stream(int **args);

int hamming74_decode_block(char *in_buffer, char *out_buffer);
void hamming74_encode_block(char in_char, char *out_buffer);

#endif //DNS_CC_FEC_H
