#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

static struct termios original_termios;
static bool terminal_is_raw;
static volatile sig_atomic_t resize_pending;
static volatile sig_atomic_t stop_requested;

static void restore_terminal(void)
{
    if (terminal_is_raw) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        terminal_is_raw = false;
    }
}

static void on_signal(int signo)
{
    if (signo == SIGWINCH) {
        resize_pending = 1;
    } else {
        stop_requested = 1;
    }
}

static int install_handler(int signo)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    return sigaction(signo, &action, NULL);
}

static int write_all(int fd, const void *data, size_t size)
{
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

static int resize_pty(int master)
{
    struct winsize size;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) < 0) {
        return -1;
    }
    return ioctl(master, TIOCSWINSZ, &size);
}

/* Ctrl-b q quits; Ctrl-b Ctrl-b sends a literal Ctrl-b to the shell. */
static int forward_input(int master, const unsigned char *data, size_t size,
                         bool *prefix, bool *quit)
{
    for (size_t i = 0; i < size; i++) {
        unsigned char byte = data[i];
        if (*prefix) {
            *prefix = false;
            if (byte == 'q') {
                *quit = true;
                return 0;
            }
            if (byte != 0x02 && write_all(master, "\x02", 1) < 0) {
                return -1;
            }
        } else if (byte == 0x02) {
            *prefix = true;
            continue;
        }
        if (write_all(master, &byte, 1) < 0) {
            return -1;
        }
    }
    return 0;
}

int main(void)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "twind: stdin and stdout must be terminals\n");
        return EXIT_FAILURE;
    }
    if (tcgetattr(STDIN_FILENO, &original_termios) < 0) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    struct winsize size;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) < 0) {
        perror("TIOCGWINSZ");
        return EXIT_FAILURE;
    }

    int master;
    pid_t child = forkpty(&master, NULL, NULL, &size);
    if (child < 0) {
        perror("forkpty");
        return EXIT_FAILURE;
    }
    if (child == 0) {
        const char *shell = getenv("SHELL");
        if (shell == NULL || *shell == '\0') {
            shell = "/bin/sh";
        }
        execl(shell, shell, (char *)NULL);
        perror("exec shell");
        _exit(127);
    }

    int result = EXIT_FAILURE;
    struct termios raw = original_termios;
    cfmakeraw(&raw);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr");
        goto cleanup;
    }
    terminal_is_raw = true;
    if (atexit(restore_terminal) != 0 ||
        install_handler(SIGWINCH) < 0 ||
        install_handler(SIGINT) < 0 ||
        install_handler(SIGTERM) < 0 ||
        install_handler(SIGHUP) < 0) {
        perror("terminal setup");
        goto cleanup;
    }

    bool prefix = false;
    bool quit = false;
    struct pollfd fds[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = master, .events = POLLIN }
    };
    while (!quit && !stop_requested) {
        if (resize_pending) {
            resize_pending = 0;
            if (resize_pty(master) < 0) {
                perror("resize PTY");
                break;
            }
        }
        int ready = poll(fds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            unsigned char buffer[4096];
            ssize_t count = read(master, buffer, sizeof(buffer));
            if (count > 0) {
                if (write_all(STDOUT_FILENO, buffer, (size_t)count) < 0) {
                    perror("write terminal");
                    break;
                }
            } else if (count == 0 || (count < 0 && errno == EIO)) {
                result = EXIT_SUCCESS;
                break;
            } else if (errno != EINTR) {
                perror("read PTY");
                break;
            }
        }
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            unsigned char buffer[4096];
            ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count > 0) {
                if (forward_input(master, buffer, (size_t)count, &prefix, &quit) < 0) {
                    perror("write PTY");
                    break;
                }
            } else if (count == 0) {
                quit = true;
            } else if (errno != EINTR) {
                perror("read terminal");
                break;
            }
        }
    }
    if (quit || stop_requested) {
        result = EXIT_SUCCESS;
    }

cleanup:
    /* forkpty gives the child its own session; signal its entire process group. */
    if (kill(-child, SIGHUP) < 0 && errno != ESRCH) {
        perror("signal shell");
    }
    close(master);
    while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {
    }
    restore_terminal();
    return result;
}
