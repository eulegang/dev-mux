#include "client.h"

#include "config.h"
#include "runtime.h"
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

static client_config *config;

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
  openlog("dev-mux-cli", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_USER);
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
