#include <errno.h>
#include <unistd.h>

#include "io.h"

int write_all(int fd, const void *data, size_t size) {
  const unsigned char *bytes = data;
  while (size != 0) {
    ssize_t written = write(fd, bytes, size);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      return -1;
    }
    bytes += written;
    size -= (size_t)written;
  }
  return 0;
}
