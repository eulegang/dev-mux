#pragma once

typedef struct reactor_t reactor_t;
typedef void (*reactor_fd_handler_t)(int fd, void *udata);
typedef void (*reactor_signal_handler_t)(int signal, void *udata);

reactor_t *reactor_init();

int reactor_add_read(reactor_t *self, int fd, void *udata,
                     reactor_fd_handler_t handler);

int reactor_add_write(reactor_t *self, int fd, void *udata,
                      reactor_fd_handler_t handler);

int reactor_add_signal(reactor_t *self, int signal, void *udata,
                       reactor_signal_handler_t handler);

void reactor_quit(reactor_t *self);

int reactor_runloop(reactor_t *self);
