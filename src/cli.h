#pragma once

#include "client/client.h"
#include "server/server.h"

int parse_server_options(server_opt_t *options, int argc, char **argv);
int parse_client_options(client_opt_t *options, int argc, char **argv);
