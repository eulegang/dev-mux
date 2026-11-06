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
      if (parse_server_options(&options, argc - 2, argv + 2) != 0) {
        return 1;
      }

      run_server(options);
    } else if (strcmp(argv[1], "client") == 0) {
      run_client();
    } else {
      fprintf(stderr, "usage: %s [server|attach]\n", argv[0]);
    }
  }

  return EXIT_FAILURE;
}
