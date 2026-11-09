#pragma once

#include <limits.h>

extern int client_socket;
extern char client_socket_path[PATH_MAX];
void client_connect(const char *buf);
