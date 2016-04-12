#include "fec.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

char hammington_generator[7][4] = {{1, 1, 0, 1},
                                   {1, 0, 1, 1},
                                   {1, 0, 0, 0},
                                   {0, 1, 1, 1},
                                   {0, 1, 0, 0},
                                   {0, 0, 1, 0},
                                   {0, 0, 0, 1}};


char hammington_verficator[3][7] = {{1, 0, 1, 0, 1, 0, 1},
                                    {0, 1, 1, 0, 0, 1, 1},
                                    {0, 0, 0, 1, 1, 1, 1}};

void fix(char *data, char *syndrome){
    if (!syndrome[0] && !syndrome[1] && !syndrome[2]){return;};
    int err_index;
    for (err_index = 0; err_index<7; err_index++){
        if ((syndrome[0] == hammington_verficator[0][err_index]) && (syndrome[1] == hammington_verficator[1][err_index]) && (syndrome[2] == hammington_verficator[2][err_index])){
            
            printf("Warning: error found in %d bit, fixing it\n", err_index+1);
            data[err_index] = !data[err_index];
            return;
        };
    }
    printf("Error could not be fixed\n");
    return;
}

void strip_parity(char *in_buff, char *out_buff){
    out_buff[0] = in_buff[2];
    out_buff[1] = in_buff[4];
    out_buff[2] = in_buff[5];
    out_buff[3] = in_buff[6];
}

void hamming74_decode_block(char *in_buffer, char *out_buffer){
    int i,j,k;
    unsigned char syndrome[3];
    unsigned char m_product;

    for(j=0; j<3; j++){
        m_product = 0;
        for(k=0; k<7; k++){
            m_product += hammington_verficator[j][k] * (unsigned char)in_buffer[k];
        }
        syndrome[j] = m_product % 2;
    }
        fix(in_buffer, &syndrome);
    strip_parity(in_buffer, out_buffer);
}
void hamming74_encode_block(char in_char, char *out_buff){
    unsigned char data_bits[4];
    unsigned char helper;
    char i, j, m_product;
    j = 0;
    for (i = 3; i >= 0; i--){
        helper = in_char << (7 - i);
        data_bits[j] = helper >>  7;
        j++;
    }
    for (i = 0; i < 7; i++){
        m_product = 0;
        for (j = 0; j < 4; j++){
            m_product += hammington_generator[i][j] * data_bits[j];
        }
        out_buff[i] = m_product % 2;
    }

}

void hamming74_decode_stream(int **args){
    int in_fd = args[0];
    int out_fd = args[1];
    unsigned char hamming_buffer[14];
    unsigned char *p_bot_hamming_buffer = hamming_buffer;
    unsigned char *p_top_hamming_buffer = &hamming_buffer[7];
    unsigned char byte_buffer[8];
    unsigned char *p_bot_byte_buffer = byte_buffer;
    unsigned char *p_top_byte_buffer = &byte_buffer[4];
    int read_len;
    char bit_index = 0;
    unsigned char in_byte;
    int i;
    unsigned char c;

    while ((read_len = read(in_fd, &in_byte, 1)) > 0){
        if (read_len == -1){
            printf("error reading stream for Error correction.\n %s", strerror(errno));
            break;
        }
        if (strncmp(&in_byte, "1", 1) == 0){
        in_byte = 1;
        } else{
        in_byte = 0;
        }
        hamming_buffer[bit_index] = in_byte;
        if (bit_index == 13){
            c = 0;
            bit_index = -1;
            hamming74_decode_block(p_top_hamming_buffer, p_top_byte_buffer);
            hamming74_decode_block(p_bot_hamming_buffer, p_bot_byte_buffer);
            for(i=0; i < 8; i++ ){
            c = c << 1;
            c |= byte_buffer[i];
            }
            write(out_fd, &c, 1);
        }
        bit_index ++;
    }
    close(out_fd);
}

void hamming74_encode_stream(int **args){
    int in_fd = (int) args[0];
    int out_fd = (int) args[1];
    unsigned char bottom_mask = 15;
    unsigned char first_half;
    unsigned char second_half;
    unsigned char hamming_buffer[14];
    unsigned char *p_first_buffer = hamming_buffer;
    unsigned char *p_seccond_buffer = &hamming_buffer[7];
    unsigned char byte_buffer[8];
    int byte_buffer_index = 0;

    int read_len;
    int write_len;
    int i;
    unsigned char in_byte;

    do{
    read_len = read(in_fd, &in_byte, 1);
    if (read_len < 0){
        strerror(errno);
        break;
    }
    first_half = in_byte >> 4;
    second_half = in_byte & bottom_mask;
    
    hamming74_encode_block(first_half, p_first_buffer);
    hamming74_encode_block(second_half, p_seccond_buffer);
    
    for (i = 0; i < 14; i++){
        if(hamming_buffer[i] == 1){
            hamming_buffer[i] = '1';
        }else{
            hamming_buffer[i] = '0';
        }
    }
    write_len = write(out_fd, hamming_buffer, 14);
    if (write_len < 0){
        strerror(errno);
        break;
    }
 
    }while(read_len == 1);
    close(out_fd);
}
