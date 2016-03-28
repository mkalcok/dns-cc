//
// Created by kauchman on 15.3.2016.
//

#ifndef DNS_CC_DISPLAY_H
#define DNS_CC_DISPLAY_H

#include <ncurses.h>

WINDOW *HISTORY_SCREEN_BORDER;
WINDOW *EDIT_SCREEN_BORDER;

WINDOW *HISTORY_SCREEN;
WINDOW *EDIT_SCREEN;

WINDOW *STATUS_SCREEN;

extern int EDIT_LINES;
extern int STATUS_LINES;

char *MY_MESSAGE;
extern size_t MY_MESSAGE_MAX;
size_t MY_MESSAGE_SIZE;

void msg_buffer_destroy();
void msg_buffer_init();
void msg_buffer_append(int c);
void msg_buffer_to_history();

void display_init();
void display_destroy();

void connect_display_input(int imput_fd);
void connect_display_output(int output_fd);

void set_status(char *status);

#endif //DNS_CC_DISPLAY_H
