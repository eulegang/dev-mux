# dev-mux

An early terminal multiplexer prototype in C.

## Build

Install `libuv`, `libgit2`, Lua (with a `lua` pkg-config module), and `pkg-config` before building.
Tests use GoogleTest; install it alongside the other build dependencies (or pass its CMake location via `CMAKE_PREFIX_PATH`).
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
The client reads `$XDG_CONFIG_HOME/dev-mux/config.lua` (or `~/.config/dev-mux/config.lua`) on attach.
The default prefix is `Ctrl-Space`; to change it, set `mux.keymap.prefix = "C-b"` in the Lua file.
A missing config file uses the default; an invalid config prevents the client from starting.
Set `DEV_MUX_TRACE=1` to redirect client stderr (including syslog messages) to `$XDG_STATE_HOME/dev-mux/client-$PID.log`, or `~/.local/state/dev-mux/client-$PID.log` if `XDG_STATE_HOME` is unset.
Press the prefix twice to send it literally. Unbound prefix keys pass through; `mux.keymap.pmap` can register prefix commands.
Exiting the shell also stops the server.
The client clears the alternate screen on attach and restores your original screen on exit.

The server accepts one attached client at a time and uses a private Unix socket in `$XDG_RUNTIME_DIR` or `$HOME`.
If the server is killed without cleaning up, remove its stale socket before restarting.
Output produced while detached is discarded; there is no screen replay yet.
Because output is passed through without VT parsing, full-screen applications that switch the alternate screen themselves may disrupt dev-mux's screen.
Next steps are a VT screen parser, multiple panes, and screen replay on reattach.
