#include <gtest/gtest.h>

extern "C" {
#include "protocol/protocol.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unistd.h>

namespace {

using Buffer = std::unique_ptr<buffer, decltype(&buffer_deinit)>;

// Restore process-wide variables even when an ASSERT_* exits a test early.
class ScopedEnv {
 public:
  explicit ScopedEnv(const char *name) : name_(name) {
    const char *value = getenv(name);
    if (value) {
      had_value_ = true;
      value_ = value;
    }
  }

  ~ScopedEnv() {
    if (had_value_) setenv(name_, value_.c_str(), 1);
    else unsetenv(name_);
  }

 private:
  const char *name_;
  bool had_value_ = false;
  std::string value_;
};

TEST(ProtocolBuffer, CompleteMessage) {
  Buffer buf(buffer_init(32), buffer_deinit);
  ASSERT_NE(buf, nullptr);
  ASSERT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_JOIN), 0);
  const char payload[] = "repo\0";
  ASSERT_EQ(buffer_push_data(buf.get(), payload, sizeof(payload)), 0);

  msg_tlv *msg = buffer_message(buf.get());
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->tag, TAG_SESSION_JOIN);
  EXPECT_EQ(msg->len, sizeof(payload));
  EXPECT_EQ(memcmp(msg->bytes, payload, sizeof(payload)), 0);
  EXPECT_EQ(buffer_uv(buf.get()).len, sizeof(msg_tlv) + sizeof(payload));

  buffer_chomp(buf.get());
  EXPECT_EQ(buf->len, 0u);
  EXPECT_EQ(buffer_message(buf.get()), nullptr);
  EXPECT_EQ(buffer_push_data(buf.get(), payload, 1), -1);
}

TEST(ProtocolBuffer, PartialRead) {
  Buffer buf(buffer_init(32), buffer_deinit);
  ASSERT_NE(buf, nullptr);
  const msg_tlv header = {TAG_SESSION_JOIN, 3};
  ASSERT_EQ(buffer_push_raw(buf.get(), (const char *)&header, sizeof(header)), 0);
  EXPECT_EQ(buffer_message(buf.get()), nullptr);
  ASSERT_EQ(buffer_push_raw(buf.get(), "ab", 2), 0);
  EXPECT_EQ(buffer_message(buf.get()), nullptr);
  ASSERT_EQ(buffer_push_raw(buf.get(), "c", 1), 0);
  ASSERT_NE(buffer_message(buf.get()), nullptr);
  EXPECT_EQ(buffer_message(buf.get())->len, 3u);
}

TEST(ProtocolBuffer, ChompFollowing) {
  Buffer buf(buffer_init(32), buffer_deinit);
  ASSERT_NE(buf, nullptr);
  ASSERT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_JOIN), 0);
  ASSERT_EQ(buffer_push_data(buf.get(), "a\0", 2), 0);
  ASSERT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_ERROR), 0);
  ASSERT_EQ(buffer_push_data(buf.get(), "bc\0", 3), 0);

  buffer_chomp(buf.get());
  msg_tlv *msg = buffer_message(buf.get());
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->tag, TAG_SESSION_ERROR);
  EXPECT_EQ(msg->len, 3u);
  EXPECT_EQ(memcmp(msg->bytes, "bc\0", 3), 0);
  buffer_chomp(buf.get());
  EXPECT_EQ(buf->len, 0u);
}

TEST(ProtocolBuffer, CapacityFailures) {
  Buffer buf(buffer_init(sizeof(msg_tlv) + 2), buffer_deinit);
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buffer_push_data(buf.get(), "x", 1), -1);
  ASSERT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_JOIN), 0);
  ASSERT_EQ(buffer_push_data(buf.get(), "ab", 2), 0);
  EXPECT_EQ(buffer_push_data(buf.get(), "c", 1), -1);
  EXPECT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_ERROR), -1);
  EXPECT_EQ(buffer_push_raw(buf.get(), "c", 1), -1);
  ASSERT_NE(buffer_message(buf.get()), nullptr);
  EXPECT_EQ(buffer_message(buf.get())->len, 2u);
  EXPECT_EQ(buf->len, sizeof(msg_tlv) + 2);
}

TEST(ProtocolSocket, RuntimeDirectory) {
  ScopedEnv runtime("XDG_RUNTIME_DIR"), home("HOME");
  ASSERT_EQ(setenv("XDG_RUNTIME_DIR", "/tmp/mux-runtime", 1), 0);
  ASSERT_EQ(setenv("HOME", "/tmp/mux-home", 1), 0);
  char path[128], expected[128];
  ASSERT_EQ(session_socket_path(path, sizeof(path)), 0);
  snprintf(expected, sizeof(expected), "/tmp/mux-runtime/dev-mux-%u.sock", (unsigned)getuid());
  EXPECT_STREQ(path, expected);
  EXPECT_EQ(session_socket_path(path, 3), -1);
}

TEST(ProtocolSocket, HomeFallback) {
  ScopedEnv runtime("XDG_RUNTIME_DIR"), home("HOME");
  ASSERT_EQ(setenv("XDG_RUNTIME_DIR", "", 1), 0);
  ASSERT_EQ(setenv("HOME", "/tmp/mux-home", 1), 0);
  char path[128];
  ASSERT_EQ(session_socket_path(path, sizeof(path)), 0);
  EXPECT_STREQ(path, "/tmp/mux-home/.dev-mux.sock");
  ASSERT_EQ(unsetenv("HOME"), 0);
  EXPECT_EQ(session_socket_path(path, sizeof(path)), -1);
}

}  // namespace
