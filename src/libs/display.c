#include "display.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

//TODO: Implement locks on writing to screens!!!!
//TODO: Implement scrollback somehow

int EDIT_LINES = 5;
int STATUS_LINES = 1;

char *MY_MESSAGE;
size_t MY_MESSAGE_MAX = 500;

void msg_buffer_destroy() {
    free(MY_MESSAGE);
    MY_MESSAGE_SIZE = 0;
    MY_MESSAGE_MAX = 500;
}

void msg_buffer_init() {
    MY_MESSAGE = calloc(MY_MESSAGE_MAX, sizeof(char));
    MY_MESSAGE_SIZE = 1;
}

void msg_buffer_append(int c) {
    char const new_char = (char) c;
    if (MY_MESSAGE_SIZE == MY_MESSAGE_MAX) {
        MY_MESSAGE_MAX += 500;
        MY_MESSAGE = realloc(MY_MESSAGE, MY_MESSAGE_MAX);
    }
    strncat(MY_MESSAGE, &new_char, 1);
    MY_MESSAGE_SIZE++;
}

void msg_buffer_to_history() {
    int old_x, old_y;
    getyx(EDIT_SCREEN, old_y, old_x);

    wprintw(HISTORY_SCREEN, "\n[Me] : %s", MY_MESSAGE);
    wrefresh(HISTORY_SCREEN);

    wmove(EDIT_SCREEN, old_y, old_x);
    wrefresh(EDIT_SCREEN);

}

void display_init() {
    initscr();
    cbreak();

    int scrn_max_x, scrn_max_y;
    getmaxyx(stdscr, scrn_max_y, scrn_max_x);

    HISTORY_SCREEN_BORDER = newwin((scrn_max_y - EDIT_LINES - STATUS_LINES - 1), scrn_max_x, 0, 0);
    EDIT_SCREEN_BORDER = newwin(EDIT_LINES + 1, scrn_max_x, (scrn_max_y - EDIT_LINES - STATUS_LINES - 1), 0);

    HISTORY_SCREEN = newwin((scrn_max_y - EDIT_LINES - STATUS_LINES - 3), scrn_max_x - 2, 1, 1);
    EDIT_SCREEN = newwin(EDIT_LINES - 1, scrn_max_x - 2, (scrn_max_y - EDIT_LINES - STATUS_LINES), 1);

    STATUS_SCREEN = newwin(STATUS_LINES, scrn_max_x, scrn_max_y - 1, 0);

    keypad(EDIT_SCREEN, true);
    scrollok(EDIT_SCREEN, true);
    idlok(EDIT_SCREEN, true);

    scrollok(HISTORY_SCREEN, true);
    idlok(HISTORY_SCREEN, true);


    box(HISTORY_SCREEN_BORDER, ACS_VLINE, ACS_HLINE);
    box(EDIT_SCREEN_BORDER, ACS_VLINE, ACS_HLINE);
    wprintw(HISTORY_SCREEN_BORDER, "Message history");
    wprintw(EDIT_SCREEN_BORDER, "New message");
    wprintw(STATUS_SCREEN, "Status: Idle");
    wmove(EDIT_SCREEN, 0, 0);

    wrefresh(HISTORY_SCREEN_BORDER);
    wrefresh(EDIT_SCREEN_BORDER);
    wrefresh(HISTORY_SCREEN);
    wrefresh(STATUS_SCREEN);
    wrefresh(EDIT_SCREEN);

}

void display_destroy() {
    endwin();
}

void connect_display_input(int input_fd) {
    int c, cur_x, cur_y;

    msg_buffer_init();

    while ((c = wgetch(EDIT_SCREEN)) != KEY_F(10)) {
        if (c == '\n') {
            getyx(EDIT_SCREEN, cur_y, cur_x);
            wmove(EDIT_SCREEN, cur_y + 1, 0);
            if (cur_y == (EDIT_LINES - 2)) {
                scroll(EDIT_SCREEN);
            }
        }
        wrefresh(EDIT_SCREEN);
        msg_buffer_append(c);
        write(input_fd, &c, 1);
    }
    msg_buffer_to_history();
    msg_buffer_destroy();
    close(input_fd);
    wclear(EDIT_SCREEN);
    wrefresh(EDIT_SCREEN);
}

void connect_display_output(int output_fd) {
    char c, old_x, old_y;
    getyx(EDIT_SCREEN, old_y, old_x);

    wprintw(HISTORY_SCREEN, "\n[Buddy] : ");
    wrefresh(HISTORY_SCREEN);
    while (read(output_fd, &c, 1) > 0) {
        wprintw(HISTORY_SCREEN, "%c", c);
        wrefresh(HISTORY_SCREEN);
    }

    wmove(EDIT_SCREEN, old_y, old_x);
    wrefresh(EDIT_SCREEN);

}

void set_status(char *status) {
    int old_x, old_y;
    getyx(EDIT_SCREEN, old_y, old_x);

    wmove(STATUS_SCREEN, 0, 0);
    wclear(STATUS_SCREEN);
    wrefresh(STATUS_SCREEN);
    wprintw(STATUS_SCREEN, "Status: %s", status);

    wmove(EDIT_SCREEN, old_y, old_x);

    wrefresh(STATUS_SCREEN);
    wrefresh(EDIT_SCREEN);
}