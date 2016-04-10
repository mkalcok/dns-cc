/* OpenSSL headers */
#include "crypto.h"
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

void handleErrors(void) {
	printf("Error!\n");
    ERR_print_errors_fp(stderr);
    abort();
}

void extract_salt(char *salt, int input_fd){
	const char salt_const[9] = "Salted__";
	char salt_header[17];
	char *p_salt_header = salt_header;
	int read_len;
    int i;
    for (i=0; i < 16; i++){
	    read_len = read(input_fd, &salt_header[i], 1);
        if (read_len != 1){
            printf("Error reading salt: %s\n", strerror(errno));
            exit(1);
        }
    }
	if(strncmp(p_salt_header, salt_const, 8)){
		// File was not salted
		// TODO: handle pushing back those 16 retireved bytes
		salt = NULL;
	}else{
		// File was salted
		strncpy(salt, &p_salt_header[8], 8);
	}
}


int fill_buffer(int fd, char *buffer, int buff_size){
	int buff_len = 0;
	char c;
	int read_len;
	char str_term = '\0';

	for(buff_len; buff_len <= buff_size; buff_len++){
		read_len = read(fd, &c, 1);
		if(read_len == 0){
			buffer[buff_len] = '\0';
			break;
		}
		buffer[buff_len] = c;
	}

	return buff_len;
}

void decrypt_stream(int input_fd, int output_fd, char *passphrase){
    /* Initialise the library */
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    OPENSSL_config(NULL);

	// IV, Key and Salt generation
	const EVP_CIPHER *cipher;
    const EVP_MD *dgst = NULL;
    unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
	unsigned char salt[8];
	unsigned char *p_salt = salt;

	// Extract salt
	extract_salt(p_salt, input_fd);

	cipher = EVP_get_cipherbyname("aes-256-cbc");
	if(!cipher) { fprintf(stderr, "no such cipher\n"); exit(1); }

	dgst=EVP_get_digestbyname("sha512");
    if(!dgst) { fprintf(stderr, "no such digest\n"); exit(1); }

    if(!EVP_BytesToKey(cipher, dgst, salt, passphrase, strlen(passphrase), 1, key, iv))
    {
        fprintf(stderr, "EVP_BytesToKey failed\n");
        exit(1);
    }


    //Decrypt the ciphertext 
    EVP_CIPHER_CTX *ctx;
    int len;
	unsigned char plaintext[512];
	unsigned char *p_plaintext = plaintext;
    int plaintext_len;

	int input_buffer_len = 256;
	char input_buffer[input_buffer_len];
	char *p_input_buffer = input_buffer;
	int buffer_index = 0;

    if (!(ctx = EVP_CIPHER_CTX_new())) handleErrors();

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors();

	do{
		buffer_index = fill_buffer(input_fd, input_buffer, input_buffer_len);
		if (1 != EVP_DecryptUpdate(ctx, p_plaintext, &len, p_input_buffer, buffer_index))
			handleErrors();
		plaintext_len = len;
		write(output_fd, p_plaintext, plaintext_len);
	}while(buffer_index == input_buffer_len);

    if (1 != EVP_DecryptFinal_ex(ctx, p_plaintext + len, &len)) handleErrors();
	write(output_fd, p_plaintext + plaintext_len, len);
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    EVP_cleanup();
    ERR_free_strings();



}

void encrypt_stream(int input_fd, int output_fd, char *passphrase){
    /* Initialise the library */
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    OPENSSL_config(NULL);

	// IV, Key and Salt generation
	const EVP_CIPHER *cipher;
    const EVP_MD *dgst = NULL;
    unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
	unsigned char salt[8];
	unsigned char *p_salt = salt;

	// Generate Random salt
	RAND_bytes(salt, 8);
	char salt_header[30]= "Salted__";
	const char *p_salt_header = salt_header;

	write(output_fd, salt_header, strlen(salt_header));
	write(output_fd, salt, sizeof(salt));

	// Generate IV and Key from passphrase	
	cipher = EVP_get_cipherbyname("aes-256-cbc");
	if(!cipher) { fprintf(stderr, "no such cipher\n"); exit(1); }

	dgst=EVP_get_digestbyname("sha512");
    if(!dgst) { fprintf(stderr, "no such digest\n"); exit(1); }

    if(!EVP_BytesToKey(cipher, dgst, salt, passphrase, strlen(passphrase), 1, key, iv))
    {
        fprintf(stderr, "EVP_BytesToKey failed\n");
        exit(1);
    }

    // Encrypt the plaintext 
    EVP_CIPHER_CTX *ctx;
	int input_buffer_len = 256;
	char input_buffer[input_buffer_len];
	char * p_input_buffer = input_buffer;
	int buffer_index;
    int len;

	unsigned char *plaintext;
	unsigned char ciphertext[512] = {};
	unsigned char *ciphertext_p = ciphertext;
	int plaintext_len;

    int ciphertext_len;
	int final_len;

    // Create and initialise the context
    if (!(ctx = EVP_CIPHER_CTX_new())) handleErrors();

   	//Initialise the encryption operation.
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors();

	// Buffer Resonable amount of data for encryption (aligned to 16)
	do{
		buffer_index = fill_buffer(input_fd, p_input_buffer, input_buffer_len);
		if (1 != EVP_EncryptUpdate(ctx, ciphertext_p, &len, p_input_buffer, buffer_index))
			handleErrors();
		ciphertext_len = len;
		write(output_fd, ciphertext_p, ciphertext_len);
	}while(buffer_index == input_buffer_len);

    //Finalise the encryption.
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext_p + len, &len)) handleErrors();

	write(output_fd, ciphertext_p + ciphertext_len, len);
	ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);
    EVP_cleanup();
    ERR_free_strings();

}

/*int main(void) {
	FILE *input_fp = fopen("/tmp/out", "r");
	FILE *output_fp = fopen("/tmp/out.dec", "w");

	int i_fd = fileno(input_fp);
	int o_fd = fileno(output_fp);
	
	char pass[10] = "hovno";
	char *p_pass = pass;

	decrypt_stream(i_fd, o_fd, p_pass);
}
*/

