#include "network.h"
#include "../protocol/protocol.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>

int server_socket = -1;
char server_socket_path[PATH_MAX] = {0};

void server_connect(const char *buf) {
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  if (buf != NULL) {
    if (strlcpy(server_socket_path, buf, PATH_MAX) >= PATH_MAX) {
      syslog(LOG_ERR, "failed to setup unix path");
      exit(EXIT_FAILURE);
    }
  } else {
    if (session_socket_path(server_socket_path, PATH_MAX) < 0) {
      syslog(LOG_ERR, "failed to setup unix path");
      exit(EXIT_FAILURE);
    }
  }

  if (strlcpy(address.sun_path, server_socket_path, sizeof(address.sun_path)) >=
      sizeof(address.sun_path)) {
    syslog(LOG_ERR, "failed to setup unix path");
    exit(EXIT_FAILURE);
  }

  server_socket = socket(AF_UNIX, SOCK_STREAM, 0);

  if (server_socket < 0) {
    syslog(LOG_ERR, "failed to open unix socket: %m");
    exit(EXIT_FAILURE);
  }

  mode_t old_mask = umask(0077);
  int res = bind(server_socket, (struct sockaddr *)&address, sizeof(address));
  umask(old_mask);
  if (res < 0) {
    syslog(LOG_ERR, "bind (session already running or stale socket?) %s: %m",
           server_socket_path);
    exit(EXIT_FAILURE);
  }

  if (listen(server_socket, 4) < 0) {
    syslog(LOG_ERR, "failed to listen for clients: %m");
    exit(EXIT_FAILURE);
  }
}
