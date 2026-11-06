#include "cli.h"

#include <getopt.h>
#include <stdio.h>

static struct option server_longopts[] = {
    {"unix", optional_argument, NULL, 'u'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int parse_server_options(server_opt_t *options, int argc, char **argv) {
  int ch;

  while ((ch = getopt_long(argc, argv, "hu:", server_longopts, NULL)) != -1) {
    switch (ch) {
    case 'u':
      options->unix = optarg;
      break;

    case 'h':
      fprintf(stdout, "dev-mux server [-u path]");
      return -1;
    }
  }

  return 0;
}
