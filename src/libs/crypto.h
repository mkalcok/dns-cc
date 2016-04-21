//
// Created by kauchman on 28.3.2016.
//

#ifndef DNS_CC_CRYPTO_H
#define DNS_CC_CRYPTO_H

int _SHA_DIGEST_LEN;

int decrypt_stream(int input_fd, int output_fd, char *passphrase);
int encrypt_stream(int input_fd, int output_fd, char *passphrase);
void get_checksum(int input_fd, unsigned char *md);
#endif //DNS_CC_CRYPTO_H
