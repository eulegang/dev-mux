#pragma once

typedef struct {
  char *unix;
} client_opt_t;

/* Returns an allocated repository root, or NULL if none was found.
 * The caller must free the returned string.
 */
char *client_repository_root(void);

int run_client(client_opt_t options);
