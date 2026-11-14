#include "cli.h"
#include "client/client.h"
#include "server/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s [server|attach]\n", argv[0]);
  } else {
    if (strcmp(argv[1], "server") == 0) {
      server_opt_t options;
      if (parse_server_options(&options, argc - 1, argv + 1) != 0) {
        return 1;
      }

      return run_server(options);
    } else if (strcmp(argv[1], "attach") == 0) {
      client_opt_t options;
      if (parse_client_options(&options, argc - 1, argv + 1) != 0) {
        return 1;
      }

      return run_client(options);
    } else {
      fprintf(stderr, "usage: %s [server|attach]\n", argv[0]);
    }
  }

  return EXIT_FAILURE;
}
