#include "keycode.h"

#include <string.h>

int parse_keycode(const char *value, keycode *result) {
  unsigned char modifiers = 0;
  const char *key = value;
  bool fused = false;

  while (!fused && *key) {
    switch (*key) {
    case 'M':
    case 'm':
      if (modifiers & KEYCODE_OPTION) {
        return -1;
      }

      modifiers |= KEYCODE_OPTION;

      break;

    case 'C':
    case 'c':
      if (modifiers & KEYCODE_CONTROL) {
        return -1;
      }

      modifiers |= KEYCODE_CONTROL;

      break;

    case '-':
      fused = true;

      break;
    }

    key++;
  }

  if (!fused || (value[0] == '-' && value[1] == 0)) {
    modifiers = 0;
    key = value;

    if (key[0] != 0 && key[1] == 0) {
      if (key[0] == '\n' || key[0] == '\x7f')
        return -1;

      result->modifiers = 0;
      result->ascii = (unsigned char)key[0];

      return 0;
    }

    if (strcasecmp(key, "space") == 0) {
      result->modifiers = modifiers;
      result->ascii = ' ';

      return 0;
    }

  } else {
    if (key[0] != 0 && key[1] == 0) {
      if (key[0] == '\n' || key[0] == '\x7f')
        return -1;

      result->modifiers = modifiers;
      result->ascii = (unsigned char)key[0];

      return 0;
    }

    if (strcasecmp(key, "space") == 0) {
      result->modifiers = modifiers;
      result->ascii = ' ';

      return 0;
    }
  }

  return -1;
}

static int parse_keycode_input_csi(size_t *len, const char **value,
                                   keycode *result) {
  const char *it = *value;
  if (it[0] != '\x1b' || it[1] != '[') {
    return -1;
  }

  it = it + 2;

  int values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  int idx = 0;

  while (*it) {
    if ('0' <= *it && *it <= '9') {
      values[idx] *= 10;
      values[idx] += *it - '0';

    } else if (*it == ':' || *it == ';') {
      idx++;
    } else if (*it == 'u') {
      it++;
      break;
    } else {
      return -1;
    }

    it++;
  }

  if (values[0] >= 256) {
    return -1;
  }

  result->ascii = (unsigned char)values[0];
  result->modifiers = 0;

  if (idx > 0) {
    int mods = values[1];
    mods -= 1;

    if (mods & 1) {
      result->ascii -= 0x20;
    }

    if (mods & 2) {
      result->modifiers |= KEYCODE_OPTION;
    }

    if (mods & 4) {
      result->modifiers |= KEYCODE_CONTROL;
    }
  }

  *len -= (size_t)(it - *value);
  *value = it;

  return 0;
}

int parse_keycode_input(size_t *len, const char **value, keycode *result) {
  if (*len == 0)
    return -1;

  if (32 <= **value && **value <= 126) {
    result->ascii = (unsigned char)**value;
    result->modifiers = 0;
    (*value)++;
    (*len)--;

    return 0;
  }

  if (**value <= 0x1a) {
    if (**value == 0) {
      result->modifiers = KEYCODE_CONTROL;
      result->ascii = ' ';
      (*value)++;
      (*len)--;

      return 0;
    }

    result->modifiers = KEYCODE_CONTROL;
    result->ascii = (unsigned char)**value + (unsigned char)0x60;
    (*value)++;
    (*len)--;

    return 0;
  }

  if (**value == 0x1b) {
    const char *it = (*value) + 1;
    if (*it == '[') {
      return parse_keycode_input_csi(len, value, result);
    } else {
      if (32 <= *it && *it <= 126) {
        result->ascii = (unsigned char)*it;
        result->modifiers = KEYCODE_OPTION;
        (*value) += 2;
        (*len) -= 2;

        return 0;
      }

      if (*it <= 0x1a) {
        if (*it == 0) {
          result->modifiers = KEYCODE_CONTROL | KEYCODE_OPTION;
          result->ascii = ' ';
          (*value) += 2;
          (*len) -= 2;

          return 0;
        }

        result->modifiers = KEYCODE_CONTROL | KEYCODE_OPTION;
        result->ascii = (unsigned char)**value + (unsigned char)0x60;
        (*value) += 2;
        (*len) -= 2;

        return 0;
      }
    }
  }
  return -1;
}
