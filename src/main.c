#include "client.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "server") == 0) {
    return run_server();
  }
  if (argc == 1 || (argc == 2 && strcmp(argv[1], "attach") == 0)) {
    return run_client();
  }
  fprintf(stderr, "usage: %s [server|attach]\n", argv[0]);
  return EXIT_FAILURE;
}
