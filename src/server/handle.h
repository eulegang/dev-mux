#pragma once

#include "../protocol/protocol.h"
#include <stdbool.h>

typedef struct server_handle server_handle;

server_handle *server_handle_init();
bool server_handle_process(server_handle *, msg_tlv *msg, buffer *out);
void server_handle_deinit(server_handle *);
