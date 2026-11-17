#pragma once

#include <stddef.h>

#define KEYCODE_CONTROL (1u << 0)
#define KEYCODE_OPTION (1u << 1)

typedef struct {
  unsigned char ascii;
  unsigned char modifiers;
} keycode;

/* Returns 0 for a valid ASCII key, -1 for an invalid key description. */
int parse_keycode(const char *value, keycode *result);

int parse_keycode_input(size_t *len, const char **value, keycode *result);
