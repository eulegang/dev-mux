#include "client.h"

#include "runtime.h"
#include "terminal.h"
#include <syslog.h>

int run_client(client_opt_t options) {
  openlog("dev-mux-cli", LOG_PID | LOG_NDELAY, LOG_USER);

  assert_termial();
  terminal_init();
  terminal_enter();

  client_runtime_start(options.unix);

  terminal_exit();
  closelog();

  return 0;
}
