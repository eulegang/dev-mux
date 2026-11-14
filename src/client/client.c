#include "client.h"

#include "runtime.h"
#include "terminal.h"
#include <git2.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

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
  openlog("dev-mux-cli", LOG_PID | LOG_NDELAY, LOG_USER);

  char *repository_root = client_repository_root();
  assert_termial();
  terminal_init();
  terminal_enter();

  client_runtime_start(options.unix, repository_root);

  terminal_exit();
  free(repository_root);
  closelog();

  return 0;
}
