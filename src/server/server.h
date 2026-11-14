#pragma once

typedef struct {
  char *unix;
  bool force;
} server_opt_t;

int run_server(server_opt_t options);
