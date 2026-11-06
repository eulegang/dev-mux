#include "client.h"

#include "../protocol/protocol.h"
#include "terminal.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static volatile sig_atomic_t resize_pending;
static volatile sig_atomic_t stop_requested;

static void on_signal(int signo) {
  if (signo == SIGWINCH) {
    resize_pending = 1;
  } else {
    stop_requested = 1;
  }
}

static int install_handler(int signo) {
  struct sigaction action = {0};
  action.sa_handler = on_signal;
  sigemptyset(&action.sa_mask);
  return sigaction(signo, &action, NULL);
}

static int send_size(int socket_fd) {
  struct winsize size;
  if (window_get_size(&size) < 0) {
    return -1;
  }
  unsigned char frame[5] = {
      MSG_RESIZE, (unsigned char)(size.ws_row >> 8), (unsigned char)size.ws_row,
      (unsigned char)(size.ws_col >> 8), (unsigned char)size.ws_col};
  return write_all(socket_fd, frame, sizeof(frame));
}

static int send_key(int socket_fd, unsigned char key) {
  unsigned char frame[2] = {MSG_INPUT, key};
  return write_all(socket_fd, frame, sizeof(frame));
}

static int forward_keys(int socket_fd, const unsigned char *data, size_t size,
                        bool *prefix, bool *done) {
  for (size_t i = 0; i < size; i++) {
    unsigned char key = data[i];
    if (*prefix) {
      *prefix = false;
      if (key == 'd' || key == 'q') {
        unsigned char command = key == 'd' ? MSG_DETACH : MSG_QUIT;
        if (write_all(socket_fd, &command, 1) < 0) {
          return -1;
        }
        *done = true;
        return 0;
      }
      if (key != 0x02 && send_key(socket_fd, 0x02) < 0) {
        return -1;
      }
    } else if (key == 0x02) {
      *prefix = true;
      continue;
    }
    if (send_key(socket_fd, key) < 0) {
      return -1;
    }
  }
  return 0;
}

int run_client(void) {
  assert_termial();
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  if (session_socket_path(address.sun_path, sizeof(address.sun_path)) < 0) {
    fprintf(stderr, "dev-mux: HOME/XDG_RUNTIME_DIR is missing or socket path "
                    "is too long\n");
    return EXIT_FAILURE;
  }
  int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }
  if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("connect (start a session with 'dev-mux server')");
    close(socket_fd);
    return EXIT_FAILURE;
  }
  terminal_init();
  struct sigaction ignore = {0};
  ignore.sa_handler = SIG_IGN;
  sigemptyset(&ignore.sa_mask);
  if (install_handler(SIGWINCH) < 0 || install_handler(SIGINT) < 0 ||
      install_handler(SIGTERM) < 0 || install_handler(SIGHUP) < 0 ||
      sigaction(SIGPIPE, &ignore, NULL) < 0) {
    perror("sigaction");
    close(socket_fd);
    return EXIT_FAILURE;
  }
  terminal_enter();
  int result = EXIT_FAILURE;
  if (send_size(socket_fd) < 0) {
    perror("send size");
    goto cleanup;
  }
  bool prefix = false;
  bool done = false;
  while (!done && !stop_requested) {
    if (resize_pending) {
      resize_pending = 0;
      if (send_size(socket_fd) < 0) {
        perror("send size");
        break;
      }
    }
    struct pollfd fds[2] = {{.fd = STDIN_FILENO, .events = POLLIN},
                            {.fd = socket_fd, .events = POLLIN}};
    if (poll(fds, 2, -1) < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      break;
    }
    if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
      unsigned char buffer[4096];
      ssize_t count = read(socket_fd, buffer, sizeof(buffer));
      if (count > 0) {
        if (write_all(STDOUT_FILENO, buffer, (size_t)count) < 0) {
          perror("write terminal");
          break;
        }
      } else if (count == 0) {
        result = EXIT_SUCCESS;
        break;
      } else if (errno != EINTR) {
        perror("read session");
        break;
      }
    }
    if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
      unsigned char buffer[4096];
      ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (count > 0) {
        if (forward_keys(socket_fd, buffer, (size_t)count, &prefix, &done) <
            0) {
          perror("send input");
          break;
        }
      } else if (count == 0) {
        done = true;
      } else if (errno != EINTR) {
        perror("read terminal");
        break;
      }
    }
  }
  if (done || stop_requested) {
    result = EXIT_SUCCESS;
  }
cleanup:
  close(socket_fd);
  terminal_exit();
  return result;
}
