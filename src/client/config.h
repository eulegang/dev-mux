#pragma once

#include "keycode.h"
#include <stdbool.h>
#include <uv.h>

typedef struct client_config client_config;

/* Loads $XDG_CONFIG_HOME/dev-mux/config.lua, or ~/.config/dev-mux/config.lua.
 * A missing file uses Ctrl-Space; other errors are reported and return NULL. */
client_config *client_config_load(void);
keycode client_config_prefix(const client_config *config);
void client_config_close(client_config *config);

bool client_config_process(const client_config *self, bool *pending,
                           const char **buf, size_t *len);

/* A prefix followed by itself sends a literal prefix. Other keys pass through
 * until command bindings are implemented. State persists across reads. */
bool client_prefix_filter(const client_config *config, bool *pending,
                          unsigned char key, unsigned char *output);
