#include "client.h"

#include "config.h"
#include "runtime.h"
#include <errno.h>
#include <fcntl.h>
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

static client_config *config;

static int client_redirect_stderr(void) {
  const char *base = getenv("XDG_STATE_HOME");
  const char *suffix = "/dev-mux";
  if (!base || !*base) {
    base = getenv("HOME");
    suffix = "/.local/state/dev-mux";
  }
  if (!base || !*base) {
    fprintf(stderr, "dev-mux: XDG_STATE_HOME and HOME are unset\n");
    return -1;
  }

  size_t dir_length = strlen(base) + strlen(suffix) + 1;
  char *dir = malloc(dir_length);
  if (!dir) {
    perror("dev-mux: allocating state directory");
    return -1;
  }
  snprintf(dir, dir_length, "%s%s", base, suffix);

  for (char *part = dir + 1;; part++) {
    if (*part != '/' && *part != '\0')
      continue;
    char saved = *part;
    *part = '\0';
    if (mkdir(dir, 0700) < 0 && errno != EEXIST) {
      perror("dev-mux: creating state directory");
      free(dir);
      return -1;
    }
    *part = saved;
    if (!saved)
      break;
  }

  size_t length = dir_length + sizeof("/client-.log") +
                  (size_t)snprintf(NULL, 0, "%ld", (long)getpid());
  char *path = malloc(length);
  if (!path) {
    perror("dev-mux: allocating log path");
    free(dir);
    return -1;
  }
  snprintf(path, length, "%s/client-%ld.log", dir, (long)getpid());
  free(dir);

  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (fd < 0 || dup2(fd, STDERR_FILENO) < 0) {
    perror(path);
    if (fd >= 0)
      close(fd);
    free(path);
    return -1;
  }
  close(fd);
  free(path);
  return 0;
}

static void client_close_config(void) { client_config_close(config); }

char *client_repository_root(void) {
  if (git_libgit2_init() < 0) {
    syslog(LOG_ERR, "failed to initialize libgit2");
    return NULL;
  }

  char *path = NULL;
  git_repository *repo = NULL;
  int result = git_repository_open_ext(&repo, ".", 0, NULL);
  if (result == 0) {
    const char *root = git_repository_workdir(repo);
    if (!root) {
      root = git_repository_path(repo);
    }
    path = malloc(strlen(root) + 1);
    if (path) {
      strcpy(path, root);
    }
    git_repository_free(repo);
  } else if (result != GIT_ENOTFOUND) {
    const git_error *error = git_error_last();
    syslog(LOG_WARNING, "failed to discover repository: %s",
           error ? error->message : "unknown error");
  }

  git_libgit2_shutdown();
  return path;
}

int run_client(client_opt_t options) {
  int syslog_opts = LOG_PID | LOG_NDELAY;
  const char *trace = getenv("DEV_MUX_TRACE");
  if (trace && strcmp(trace, "1") == 0) {
    syslog_opts |= LOG_PERROR;
    if (client_redirect_stderr() != 0)
      return EXIT_FAILURE;
  }

  openlog("dev-mux-cli", syslog_opts, LOG_USER);
  config = client_config_load();
  if (!config) {
    closelog();
    return EXIT_FAILURE;
  }
  if (atexit(client_close_config) != 0) {
    client_config_close(config);
    closelog();
    return EXIT_FAILURE;
  }

  char *repository_root = client_repository_root();

  if (!repository_root) {
    fprintf(stderr, "trying to attach without a repository\n");
    syslog(LOG_ERR, "trying to attach without a repository");
    exit(EXIT_FAILURE);
  }

  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    fprintf(stderr, "dev-mux: stdin and stdout must be terminals\n");
    exit(EXIT_FAILURE);
  }

  client_runtime_start(options.unix, repository_root, config);

  free(repository_root);
  closelog();

  return 0;
}
