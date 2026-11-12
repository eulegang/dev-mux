#include "reactor.h"

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/event.h>
#include <syslog.h>
#include <unistd.h>

#define REACTOR_RELOAD_SIGNAL SIGHUP

static void reactor_signal_handler(int signo) {}

atomic_bool reactor_system_initialized = false;
static void initialize_rector_system() {
  bool cur = false;
  if (!atomic_compare_exchange_strong(&reactor_system_initialized, &cur,
                                      true)) {
    struct sigaction action = {0};
    action.sa_handler = reactor_signal_handler;
    sigemptyset(&action.sa_mask);
    sigaction(REACTOR_RELOAD_SIGNAL, &action, NULL);
  }
}

struct reactor_t {
  int queue;

  atomic_bool quit;
};

reactor_t *reactor_init() {
  initialize_rector_system();

  reactor_t *self = malloc(sizeof(reactor_t));
  if (self == NULL) {
    return NULL;
  }

  int queue = kqueue();

  if (queue < 0) {
    syslog(LOG_ERR, "failed to initialize reactor: %m");
    free(self);
    return NULL;
  }

  self->queue = queue;
  self->quit = false;

  syslog(LOG_INFO, "initialized reactor");
  return self;
}

int reactor_add_read(reactor_t *self, int fd, void *udata,
                     reactor_fd_handler_t handler) {
  syslog(LOG_INFO, "adding %d for read filter", fd);
  struct kevent change;

  change.ident = fd;
  change.filter = EVFILT_READ;
  change.flags = EV_ADD;
  change.fflags = 0;
  change.data = 0;
  change.udata = udata;

  int n = kevent(self->queue, &change, 1, NULL, 0, NULL);

  if (n < 0) {
    syslog(LOG_ERR, "failed to add read file to reactor: %m");
  }

  return n;
}

int reactor_add_write(reactor_t *self, int fd, void *udata,
                      reactor_fd_handler_t handler) {
  syslog(LOG_INFO, "adding %d for write filter", fd);
  struct kevent change;

  change.ident = fd;
  change.filter = EVFILT_WRITE;
  change.flags = EV_ADD;
  change.fflags = 0;
  change.data = 0;
  change.udata = udata;

  int n = kevent(self->queue, &change, 1, NULL, 0, NULL);

  if (n < 0) {
    syslog(LOG_ERR, "failed to add write file to reactor: %m");
  }

  return n;
}

int reactor_add_signal(reactor_t *self, int signal, void *udata,
                       reactor_signal_handler_t handler) {
  syslog(LOG_INFO, "adding %d for signal filter", signal);
  struct kevent change;

  change.ident = signal;
  change.filter = EVFILT_SIGNAL;
  change.flags = EV_ADD;
  change.fflags = 0;
  change.data = 0;
  change.udata = udata;

  int n = kevent(self->queue, &change, 1, NULL, 0, NULL);

  if (n < 0) {
    syslog(LOG_ERR, "failed to add signal to reactor: %m");
  }

  return n;
}

void reactor_quit(reactor_t *self) {
  atomic_store(&self->quit, true);
  pid_t pid = getpid();
  if (pid > 0) {
    kill(pid, REACTOR_RELOAD_SIGNAL);
  }
}

int reactor_runloop(reactor_t *self) {
  syslog(LOG_INFO, "starting reactor");
  while (!atomic_load(&self->quit)) {
    struct kevent events[16];

    int n = kevent(self->queue, NULL, 0, events, 16, NULL);

    if (n < 0) {
      syslog(LOG_ERR, "failed to monitor %m");
      return n;
    }

    for (int i = 0; i < n; i++) {
      struct kevent e = events[i];

      switch (e.filter) {
      case EVFILT_READ: {
        reactor_fd_handler_t handle = (reactor_fd_handler_t)e.udata;

        handle(e.ident, e.udata);
      } break;

      case EVFILT_WRITE: {
        reactor_fd_handler_t handle = (reactor_fd_handler_t)e.udata;

        handle(e.ident, e.udata);
      } break;

      case EVFILT_SIGNAL: {
        reactor_signal_handler_t handle = (reactor_signal_handler_t)e.udata;

        handle(e.ident, e.udata);
      } break;

      default:
        syslog(LOG_WARNING, "unknown kqueue filter found");
        break;
      }
    }
  }
}
