#pragma once

#include <limits.h>

extern int server_socket;
extern char server_socket_path[PATH_MAX];
void server_connect(const char *buf);
