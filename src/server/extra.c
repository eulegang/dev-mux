

// void server_run_pane() {
//
// struct winsize initial_size = {.ws_row = 24, .ws_col = 80};
// child = forkpty(&master, NULL, NULL, &initial_size);
// if (child < 0) {
//   perror("forkpty");
//   goto cleanup;
// }
// if (child == 0) {
//   closelog();
//   close(listener);
//   const char *shell = getenv("SHELL");
//   if (shell == NULL || *shell == '\0') {
//     shell = "/bin/sh";
//   }
//   execl(shell, shell, (char *)NULL);
//   perror("exec shell");
//   _exit(127);
// }
//
// }

// if (client >= 0) {
//   close(client);
// }
// if (child > 0) {
//   if (kill(-child, SIGHUP) < 0 && errno != ESRCH) {
//     perror("signal shell");
//   }
//   close(master);
//   while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {
//   }
// }
//

//
//
//
// void server_runtime() {
//
// unsigned char frame[5];
// while (!stopping) {
//   struct pollfd fds[3] = {
//       {.fd = listener, .events = POLLIN},
//       {.fd = master, .events = POLLIN},
//       {.fd = client, .events = POLLIN},
//   };
//   if (poll(fds, 3, -1) < 0) {
//     if (errno == EINTR) {
//       continue;
//     }
//     perror("poll");
//     break;
//   }
//   if (fds[0].revents & POLLIN) {
//     int incoming = accept(listener, NULL, NULL);
//     if (incoming >= 0) {
//       if (client >= 0) {
//         close(incoming); /* This prototype supports one attached client. */
//       } else {
//         client = incoming;
//         have = 0;
//       }
//     } else if (errno != EINTR) {
//       perror("accept");
//       break;
//     }
//   }
//   if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
//     unsigned char buffer[4096];
//     ssize_t count = read(master, buffer, sizeof(buffer));
//     if (count > 0) {
//       if (client >= 0 && write_all(client, buffer, (size_t)count) < 0) {
//         disconnect_client(&client, &have);
//       }
//     } else if (count == 0 || (count < 0 && errno == EIO)) {
//       result = EXIT_SUCCESS;
//       break;
//     } else if (errno != EINTR) {
//       perror("read PTY");
//       break;
//     }
//   }
//   if (client >= 0 && fds[2].fd == client &&
//       (fds[2].revents & (POLLIN | POLLHUP | POLLERR))) {
//     unsigned char buffer[4096];
//     ssize_t count = read(client, buffer, sizeof(buffer));
//     if (count <= 0) {
//       if (count < 0 && errno == EINTR) {
//         continue;
//       }
//       disconnect_client(&client, &have);
//     } else {
//       for (ssize_t i = 0; i < count; i++) {
//         frame[have++] = buffer[i];
//         size_t needed = frame[0] == MSG_RESIZE  ? 5
//                         : frame[0] == MSG_INPUT ? 2
//                                                 : 1;
//         if (have != needed) {
//           continue;
//         }
//         bool quit = false, detach = false;
//         if (handle_message(frame, master, &quit, &detach) < 0) {
//           perror("client message");
//           disconnect_client(&client, &have);
//           break;
//         }
//         have = 0;
//         if (quit) {
//           stopping = 1;
//           break;
//         }
//         if (detach) {
//           disconnect_client(&client, &have);
//           break;
//         }
//       }
//     }
//   }
// }
// if (stopping) {
//   result = EXIT_SUCCESS;
// }
//
//}
//
//
//
// static void disconnect_client(int *client, size_t *have) {
//   if (*client >= 0) {
//     close(*client);
//     *client = -1;
//   }
//   *have = 0;
// }
//
// static int handle_message(const unsigned char *frame, int master, bool *quit,
//                           bool *detach) {
//   if (frame[0] == MSG_INPUT) {
//     return write_all(master, frame + 1, 1);
//   }
//   if (frame[0] == MSG_RESIZE) {
//     struct winsize size = {0};
//     size.ws_row = (unsigned short)((frame[1] << 8) | frame[2]);
//     size.ws_col = (unsigned short)((frame[3] << 8) | frame[4]);
//     return ioctl(master, TIOCSWINSZ, &size);
//   }
//   if (frame[0] == MSG_DETACH) {
//     *detach = true;
//   } else if (frame[0] == MSG_QUIT) {
//     *quit = true;
//   }
//   return 0;
// }
//
