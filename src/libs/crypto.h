//
// Created by kauchman on 28.3.2016.
//

#ifndef DNS_CC_CRYPTO_H
#define DNS_CC_CRYPTO_H

void decrypt_stream(int input_fd, int output_fd, char *passphrase);
void encrypt_stream(int input_fd, int output_fd, char *passphrase);
#endif //DNS_CC_CRYPTO_H
