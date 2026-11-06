#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int session_socket_path(char *buffer, size_t capacity) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  const char *home = getenv("HOME");
  int length;
  if (runtime != NULL && *runtime != '\0') {
    length = snprintf(buffer, capacity, "%s/twind-%lu.sock", runtime,
                      (unsigned long)getuid());
  } else if (home != NULL && *home != '\0') {
    length = snprintf(buffer, capacity, "%s/.twind.sock", home);
  } else {
    return -1;
  }
  return length >= 0 && (size_t)length < capacity ? 0 : -1;
}
