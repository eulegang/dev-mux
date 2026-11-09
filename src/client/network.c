#include "network.h"
#include "../protocol/protocol.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

int client_socket = -1;
char client_socket_path[PATH_MAX] = {0};

void client_connect(const char *buf) {
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  if (buf != NULL) {
    if (strlcpy(client_socket_path, buf, PATH_MAX) >= PATH_MAX) {
      syslog(LOG_ERR, "failed to setup unix path");
      exit(EXIT_FAILURE);
    }
  } else {
    if (session_socket_path(client_socket_path, PATH_MAX) < 0) {
      syslog(LOG_ERR, "failed to setup unix path");
      exit(EXIT_FAILURE);
    }
  }

  if (strlcpy(address.sun_path, client_socket_path, sizeof(address.sun_path)) >=
      sizeof(address.sun_path)) {
    syslog(LOG_ERR, "failed to setup unix path");
    exit(EXIT_FAILURE);
  }

  client_socket = socket(AF_UNIX, SOCK_STREAM, 0);

  if (client_socket < 0) {
    syslog(LOG_ERR, "failed to open unix socket: %m");
    exit(EXIT_FAILURE);
  }

  if (connect(client_socket, (struct sockaddr *)&address, sizeof(address)) <
      0) {
    syslog(LOG_ERR, "connect to a session 'dev-mux attach'");
    close(client_socket);
    exit(EXIT_FAILURE);
  }
}
