#include "runtime.h"
#include "handle.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

static int server_queue = -1;

struct runtime_handler {
  int fd;
  void (*handle)(struct runtime_handler *, struct kevent event);
  void *udata;
};

static void server_runtime_client_handler(struct runtime_handler *handler,
                                          struct kevent event) {
  (void)event;
  server_handle *handle = handler->udata;

  server_handle_read(handle);
}

static void server_runtime_listener_handler(struct runtime_handler *handler,
                                            struct kevent event) {
  (void)event;

  int fd = accept(handler->fd, NULL, NULL);
  if (fd == -1)
    return;

  struct runtime_handler *client_handler =
      malloc(sizeof(struct runtime_handler));

  if (!client_handler) {
    goto free_handler;
  }

  client_handler->fd = fd;
  client_handler->handle = server_runtime_client_handler;
  client_handler->udata = server_handle_init(fd);

  if (!client_handler->udata) {
    goto free_handle;
  }

  struct kevent change;
  EV_SET(&change, fd, EVFILT_READ, EV_ADD, 0, 0, client_handler);

  if (kevent(server_queue, &change, 1, NULL, 0, NULL) < 0) {
    goto free_handle;
  }

  return;

free_handle:
  server_handle_deinit(client_handler->udata);

free_handler:
  free(client_handler);

  syslog(LOG_ERR, "failed to setup client handler");
  close(fd);
}

int server_runtime_init(int listener) {
  server_queue = kqueue();

  if (server_queue == -1) {
    syslog(LOG_ERR, "failed to listen to event queue: %m");
    return -1;
  }

  struct runtime_handler *handler = malloc(sizeof(struct runtime_handler));
  if (!handler) {
    syslog(LOG_ERR, "failed to allocate listener runtime handler");
    return -1;
  }

  handler->fd = listener;
  handler->handle = server_runtime_listener_handler;
  handler->udata = NULL;

  struct kevent changes[3];
  EV_SET(&changes[0], listener, EVFILT_READ, EV_ADD, 0, 0, handler);
  EV_SET(&changes[1], SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
  EV_SET(&changes[2], SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

  if (kevent(server_queue, changes, 3, NULL, 0, NULL) < 0) {
    syslog(LOG_ERR, "failed to setup listener: %m");
    return -1;
  }

  return 0;
}

void server_runtime_run() {
  while (1) {
    struct kevent event;

    int n = kevent(server_queue, NULL, 0, &event, 1, NULL);

    if (n < 0) {
      syslog(LOG_ERR, "failed to wait on queue: %m");
      return;
    }

    if (n != 1) {
      continue;
    }

    if (event.filter == EVFILT_SIGNAL) {
      if (event.ident == SIGTERM || event.ident == SIGINT) {
        syslog(LOG_INFO, "gracefully shuting down");
        return;
      }

    } else if (event.filter == EVFILT_READ && event.udata) {
      struct runtime_handler *handler = event.udata;
      handler->handle(handler, event);
    }
  }
}

void server_runtime_deinit() {
  if (server_queue != -1) {
    close(server_queue);
  }
}
