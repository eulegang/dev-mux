# twind

An early terminal multiplexer prototype in C.

## Build

```sh
cmake -S . -B build
cmake --build build
./build/twind
```

Run it from a terminal, not a pipe.
It starts your `$SHELL` (or `/bin/sh`) in a pseudo-terminal and forwards input and output.
Press `Ctrl-b q` to quit, or `Ctrl-b Ctrl-b` to send a literal `Ctrl-b` to the shell.
Exiting the shell also exits twind.

This is the PTY foundation, not yet a pane-based multiplexer.
Next steps are multiple PTYs, a VT screen parser for each pane, layout and rendering, and finally an attachable session server.
