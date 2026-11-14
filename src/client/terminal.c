#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "terminal.h"

struct winsize term_size;

static struct termios entered_term;

static bool term_is_raw = false;
static bool term_is_alt = false;

static int write_all(int fd, const void *data, size_t size) {
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

void assert_termial() {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    fprintf(stderr, "dev-mux: stdin and stdout must be terminals\n");
    exit(EXIT_FAILURE);
  }
}

int window_get_size(struct winsize *size) {
  return ioctl(STDIN_FILENO, TIOCGWINSZ, size);
}

int window_resize_pty(int master) {
  struct winsize size;
  if (window_get_size(&size) < 0) {
    return -1;
  }
  return ioctl(master, TIOCSWINSZ, &size);
}

void terminal_init() {
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &term_size) < 0) {
    perror("failed to get window size");
    exit(1);
  }

  if (tcgetattr(STDIN_FILENO, &entered_term) < 0) {
    perror("tcgetattr");
    exit(1);
  }
}

void terminal_enter() {
  struct termios raw = entered_term;

  cfmakeraw(&raw);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
    perror("tcsetattr");
    exit(1);
  }

  term_is_raw = true;

  if (atexit(terminal_exit) != 0) {
    perror("failed to hook exit terminal");
    exit(1);
  }

  static const char enter[] = "\x1b[?1049h\x1b[2J\x1b[H";
  if (write_all(STDOUT_FILENO, enter, sizeof(enter) - 1) < 0) {
    perror("enter alternate screen");
    exit(1);
  }

  term_is_alt = true;
}

void terminal_exit() {
  if (term_is_alt) {
    static const char leave[] = "\x1b[?1049l";
    write_all(STDOUT_FILENO, leave, sizeof(leave) - 1);
    term_is_alt = false;
  }

  if (term_is_raw) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &entered_term);
    term_is_raw = false;
  }
}
