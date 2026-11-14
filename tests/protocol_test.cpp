#include <gtest/gtest.h>

extern "C" {
#include "protocol/protocol.h"
}

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unistd.h>

namespace {
using BufferPtr = std::unique_ptr<buffer, decltype(&buffer_deinit)>;

BufferPtr make_buffer(uint16_t capacity) {
  return BufferPtr(buffer_init(capacity), buffer_deinit);
}

class ScopedEnv {
 public:
  ScopedEnv(const char *name, const char *value) : name_(name) {
    const char *old = std::getenv(name);
    if (old) {
      old_value_ = old;
      had_value_ = true;
    }
    if (value)
      setenv(name, value, 1);
    else
      unsetenv(name);
  }

  ~ScopedEnv() {
    if (had_value_)
      setenv(name_, old_value_.c_str(), 1);
    else
      unsetenv(name_);
  }

 private:
  const char *name_;
  std::string old_value_;
  bool had_value_ = false;
};
}  // namespace

TEST(ProtocolBuffer, CompleteMessageAndChomp) {
  auto buf = make_buffer(32);
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_HELLO), 0);
  const char payload[] = "repo";
  EXPECT_EQ(buffer_push_data(buf.get(), payload, sizeof(payload)), 0);

  auto *msg = buffer_message(buf.get());
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->tag, TAG_SESSION_HELLO);
  EXPECT_EQ(msg->len, sizeof(payload));
  EXPECT_EQ(std::memcmp(msg->bytes, payload, sizeof(payload)), 0);
  EXPECT_EQ(buffer_uv(buf.get()).len, sizeof(msg_tlv) + sizeof(payload));

  buffer_chomp(buf.get());
  EXPECT_EQ(buf->len, 0);
  EXPECT_EQ(buffer_message(buf.get()), nullptr);
  EXPECT_EQ(buffer_push_data(buf.get(), payload, 1), -1);
}

TEST(ProtocolBuffer, PartialReadWaitsForFullMessage) {
  auto buf = make_buffer(32);
  ASSERT_NE(buf, nullptr);
  const msg_tlv header = {TAG_SESSION_HELLO, 3};
  ASSERT_EQ(buffer_push_raw(buf.get(), reinterpret_cast<const char *>(&header),
                            sizeof(header)), 0);
  EXPECT_EQ(buffer_message(buf.get()), nullptr);
  ASSERT_EQ(buffer_push_raw(buf.get(), "ab", 2), 0);
  EXPECT_EQ(buffer_message(buf.get()), nullptr);
  ASSERT_EQ(buffer_push_raw(buf.get(), "c", 1), 0);
  ASSERT_NE(buffer_message(buf.get()), nullptr);
  EXPECT_EQ(buffer_message(buf.get())->len, 3);
}

TEST(ProtocolBuffer, ChompPreservesFollowingMessage) {
  auto buf = make_buffer(32);
  ASSERT_NE(buf, nullptr);
  ASSERT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_HELLO), 0);
  ASSERT_EQ(buffer_push_data(buf.get(), "a", 1), 0);
  ASSERT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_ERROR), 0);
  ASSERT_EQ(buffer_push_data(buf.get(), "bc", 2), 0);

  buffer_chomp(buf.get());
  auto *msg = buffer_message(buf.get());
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->tag, TAG_SESSION_ERROR);
  EXPECT_EQ(msg->len, 2);
  EXPECT_EQ(std::memcmp(msg->bytes, "bc", 2), 0);
  buffer_chomp(buf.get());
  EXPECT_EQ(buf->len, 0);
}

TEST(ProtocolBuffer, CapacityFailuresDoNotModifyBuffer) {
  auto buf = make_buffer(sizeof(msg_tlv) + 2);
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buffer_push_data(buf.get(), "x", 1), -1);
  ASSERT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_HELLO), 0);
  ASSERT_EQ(buffer_push_data(buf.get(), "ab", 2), 0);
  EXPECT_EQ(buffer_push_data(buf.get(), "c", 1), -1);
  EXPECT_EQ(buffer_push_msg(buf.get(), TAG_SESSION_ERROR), -1);
  EXPECT_EQ(buffer_push_raw(buf.get(), "c", 1), -1);
  ASSERT_NE(buffer_message(buf.get()), nullptr);
  EXPECT_EQ(buffer_message(buf.get())->len, 2);
  EXPECT_EQ(buf->len, sizeof(msg_tlv) + 2);
}

TEST(SocketPath, PrefersRuntimeDirectory) {
  ScopedEnv runtime("XDG_RUNTIME_DIR", "/tmp/mux-runtime");
  ScopedEnv home("HOME", "/tmp/mux-home");
  char path[128];
  ASSERT_EQ(session_socket_path(path, sizeof(path)), 0);
  EXPECT_EQ(std::string(path), "/tmp/mux-runtime/dev-mux-" +
                                   std::to_string(getuid()) + ".sock");
  EXPECT_EQ(session_socket_path(path, 3), -1);
}

TEST(SocketPath, FallsBackToHomeAndRejectsMissingDirectories) {
  ScopedEnv runtime("XDG_RUNTIME_DIR", "");
  ScopedEnv home("HOME", "/tmp/mux-home");
  char path[128];
  ASSERT_EQ(session_socket_path(path, sizeof(path)), 0);
  EXPECT_STREQ(path, "/tmp/mux-home/.dev-mux.sock");

  unsetenv("HOME");
  EXPECT_EQ(session_socket_path(path, sizeof(path)), -1);
}
