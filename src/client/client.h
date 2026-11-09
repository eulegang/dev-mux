#pragma once

typedef struct {
  char *unix;
} client_opt_t;

int run_client(client_opt_t options);
