
// static int send_size(int socket_fd) {
//   struct winsize size;
//   if (window_get_size(&size) < 0) {
//     return -1;
//   }
//   unsigned char frame[5] = {
//       MSG_RESIZE, (unsigned char)(size.ws_row >> 8), (unsigned
//       char)size.ws_row, (unsigned char)(size.ws_col >> 8), (unsigned
//       char)size.ws_col};
//   return write_all(socket_fd, frame, sizeof(frame));
// }
//
// static int send_key(int socket_fd, unsigned char key) {
//   unsigned char frame[2] = {MSG_INPUT, key};
//   return write_all(socket_fd, frame, sizeof(frame));
// }
//
// static int forward_keys(int socket_fd, const unsigned char *data, size_t
// size,
//                         bool *prefix, bool *done) {
//   for (size_t i = 0; i < size; i++) {
//     unsigned char key = data[i];
//     if (*prefix) {
//       *prefix = false;
//       if (key == 'd' || key == 'q') {
//         unsigned char command = key == 'd' ? MSG_DETACH : MSG_QUIT;
//         if (write_all(socket_fd, &command, 1) < 0) {
//           return -1;
//         }
//         *done = true;
//         return 0;
//       }
//       if (key != 0x02 && send_key(socket_fd, 0x02) < 0) {
//         return -1;
//       }
//     } else if (key == 0x02) {
//       *prefix = true;
//       continue;
//     }
//     if (send_key(socket_fd, key) < 0) {
//       return -1;
//     }
//   }
//   return 0;
// }
//
// if (send_size(socket_fd) < 0) {
//   perror("send size");
//   goto cleanup;
// }
// bool prefix = false;
// bool done = false;
// while (!done && !stop_requested) {
//   if (resize_pending) {
//     resize_pending = 0;
//     if (send_size(socket_fd) < 0) {
//       perror("send size");
//       break;
//     }
//   }
//   struct pollfd fds[2] = {{.fd = STDIN_FILENO, .events = POLLIN},
//                           {.fd = socket_fd, .events = POLLIN}};
//   if (poll(fds, 2, -1) < 0) {
//     if (errno == EINTR) {
//       continue;
//     }
//     perror("poll");
//     break;
//   }
//   if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
//     unsigned char buffer[4096];
//     ssize_t count = read(socket_fd, buffer, sizeof(buffer));
//     if (count > 0) {
//       if (write_all(STDOUT_FILENO, buffer, (size_t)count) < 0) {
//         perror("write terminal");
//         break;
//       }
//     } else if (count == 0) {
//       result = EXIT_SUCCESS;
//       break;
//     } else if (errno != EINTR) {
//       perror("read session");
//       break;
//     }
//   }
//   if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
//     unsigned char buffer[4096];
//     ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
//     if (count > 0) {
//       if (forward_keys(socket_fd, buffer, (size_t)count, &prefix, &done) <
//           0) {
//         perror("send input");
//         break;
//       }
//     } else if (count == 0) {
//       done = true;
//     } else if (errno != EINTR) {
//       perror("read terminal");
//       break;
//     }
//   }
// }
// if (done || stop_requested) {
//   result = EXIT_SUCCESS;
// }
