#include "protocol.h"

#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

int socket_fd = -1;
char socket_path[PATH_MAX];
bool socket_remove_path = false;

int socket_connect(char *path) {
  struct sockaddr_un address = {.sun_family = AF_UNIX};

  if (strlcpy(socket_path, path, PATH_MAX) >= PATH_MAX) {
    syslog(LOG_ERR, "failed to setup unix path");
    return -1;
  }

  if (strlcpy(address.sun_path, socket_path, sizeof(address.sun_path)) >=
      sizeof(address.sun_path)) {
    syslog(LOG_ERR, "failed to setup unix path");
    return -1;
  }

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (socket_fd < 0) {
    syslog(LOG_ERR, "failed to open unix socket: %m");
    return -1;
  }

  if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    syslog(LOG_ERR, "connect to a session 'dev-mux attach'");
    close(socket_fd);
    return -1;
  }

  return socket_fd;
}

int socket_serve(char *path) {

  struct sockaddr_un address = {.sun_family = AF_UNIX};
  if (strlcpy(socket_path, path, PATH_MAX) >= PATH_MAX) {
    syslog(LOG_ERR, "failed to setup unix path");
    return -1;
  }
  if (strlcpy(address.sun_path, socket_path, sizeof(address.sun_path)) >=
      sizeof(address.sun_path)) {
    syslog(LOG_ERR, "failed to setup unix path");
    return -1;
  }

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (socket_fd < 0) {
    syslog(LOG_ERR, "failed to open unix socket: %m");
    return socket_fd;
  }

  mode_t old_mask = umask(0077);
  int res = bind(socket_fd, (struct sockaddr *)&address, sizeof(address));
  umask(old_mask);
  if (res < 0) {
    syslog(LOG_ERR, "bind (session already running or stale socket?) %s: %m",
           socket_path);
    return -1;
  }

  if (listen(socket_fd, 4) < 0) {
    syslog(LOG_ERR, "failed to listen for clients: %m");
    return -1;
  }

  socket_remove_path = true;

  return socket_fd;
}

void socket_deinit() {
  if (socket_fd >= 0) {
    close(socket_fd);
  }

  if (*socket_path != 0) {
    unlink(socket_path);
  }

  return;
}
