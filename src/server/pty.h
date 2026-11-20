#pragma once

#include <uv.h>

// Start a shell on a PTY and register its master with the server loop.
// The returned handles are owned by the loop and closed by the server's uv_walk.
int server_pty_start(uv_loop_t *loop);
void server_pty_stop(void);
