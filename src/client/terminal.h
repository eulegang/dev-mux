#ifndef DEV_MUX_TERMINAL_H
#define DEV_MUX_TERMINAL_H

#include <sys/ioctl.h>

extern struct winsize term_size;

int window_get_size(struct winsize *size);
int window_resize_pty(int master);

void assert_termial();

void terminal_init();
void terminal_enter();
void terminal_exit();

#endif
