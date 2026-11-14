#include "client.h"

#include "terminal.h"

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

int run_client(client_opt_t) {
  openlog("dev-mux-cli", LOG_PID | LOG_NDELAY, LOG_USER);

  assert_termial();
  terminal_init();
  terminal_enter();

  terminal_exit();

  return 0;
}
