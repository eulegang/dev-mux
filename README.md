# dev-mux

An early terminal multiplexer prototype in C.

## Build

Install `libuv`, `libgit2`, `pkg-config`, and GoogleTest before building.
Pass `-DBUILD_TESTING=OFF` to CMake if you only want the application.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

Start the server in one terminal (or in the background):

```sh
./build/dev-mux server
```

Attach from another terminal:

```sh
./build/dev-mux attach
```

`./build/dev-mux` also attaches.
Press `Ctrl-b d` to detach and keep the shell running; attach again later to reuse it.
Press `Ctrl-b q` to stop the server and shell, or `Ctrl-b Ctrl-b` to send a literal `Ctrl-b` to the shell.
Exiting the shell also stops the server.
The client clears the alternate screen on attach and restores your original screen on exit.

The server accepts one attached client at a time and uses a private Unix socket in `$XDG_RUNTIME_DIR` or `$HOME`.
If the server is killed without cleaning up, remove its stale socket before restarting.
Output produced while detached is discarded; there is no screen replay yet.
Because output is passed through without VT parsing, full-screen applications that switch the alternate screen themselves may disrupt dev-mux's screen.
Next steps are a VT screen parser, multiple panes, and screen replay on reattach.
