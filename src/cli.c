#include "cli.h"
#include "protocol/protocol.h"

#include <getopt.h>
#include <stdio.h>

#include <limits.h>

char common_unix_path[PATH_MAX];

static struct option server_longopts[] = {
    {"unix", optional_argument, NULL, 'u'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int parse_server_options(server_opt_t *options, int argc, char **argv) {
  options->unix = NULL;

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

  if (!options->unix) {
    if (session_socket_path(common_unix_path, PATH_MAX) < 0) {
      fprintf(stderr, "failed to parse unix path");
      return -1;
    }

    options->unix = common_unix_path;
  }

  return 0;
}

static struct option client_longopts[] = {
    {"unix", optional_argument, NULL, 'u'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int parse_client_options(client_opt_t *options, int argc, char **argv) {
  options->unix = NULL;

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

  if (!options->unix) {
    if (session_socket_path(common_unix_path, PATH_MAX) < 0) {
      fprintf(stderr, "failed to parse unix path");
      return -1;
    }

    options->unix = common_unix_path;
  }

  return 0;
}
