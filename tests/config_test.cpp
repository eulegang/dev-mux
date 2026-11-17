#include <gtest/gtest.h>

extern "C" {
#include "../src/client/config.h"
}

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

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

struct TempConfigDir {
  char dir[sizeof("/tmp/dev-mux-config-XXXXXX")] = "/tmp/dev-mux-config-XXXXXX";
  bool created = false;
  std::string folder;
  std::string path;

  ~TempConfigDir() {
    if (!path.empty()) unlink(path.c_str());
    if (!folder.empty()) rmdir(folder.c_str());
    if (created) rmdir(dir);
  }
};

using Config = std::unique_ptr<client_config, decltype(&client_config_close)>;

static void save(const char *path, const char *content) {
  FILE *file = fopen(path, "w");
  ASSERT_NE(file, nullptr);
  EXPECT_GE(fputs(content, file), 0);
  EXPECT_EQ(fclose(file), 0);
}

TEST(Config, LoadAndPrefix) {
  TempConfigDir temp;
  ASSERT_NE(mkdtemp(temp.dir), nullptr);
  temp.created = true;
  ScopedEnv config_home("XDG_CONFIG_HOME");
  ASSERT_EQ(setenv("XDG_CONFIG_HOME", temp.dir, 1), 0);

  Config config(client_config_load(), client_config_close);
  ASSERT_NE(config, nullptr);
  EXPECT_EQ(client_config_prefix(config.get()).ascii, 0u);
  config.reset();

  temp.folder = std::string(temp.dir) + "/dev-mux";
  ASSERT_EQ(mkdir(temp.folder.c_str(), 0700), 0);
  temp.path = temp.folder + "/config.lua";
  save(temp.path.c_str(), "mux.keymap.prefix = 'C-b'\n");
  config.reset(client_config_load());
  ASSERT_NE(config, nullptr);
  EXPECT_EQ(client_config_prefix(config.get()).ascii, 'b');
  EXPECT_EQ(client_config_prefix(config.get()).modifiers, KEYCODE_CONTROL);
  bool custom_pending = false;
  const char ctrl_b[] = "\x02";
  const char *input = ctrl_b;
  size_t len = sizeof(ctrl_b) - 1;
  EXPECT_FALSE(client_config_process(config.get(), &custom_pending, &input, &len));
  EXPECT_TRUE(custom_pending);
  EXPECT_EQ(len, 0u);
  input = ctrl_b;
  len = sizeof(ctrl_b) - 1;
  EXPECT_TRUE(client_config_process(config.get(), &custom_pending, &input, &len));
  EXPECT_EQ(len, 0u);
  EXPECT_FALSE(custom_pending);
  config.reset();

  save(temp.path.c_str(), "mux.keymap.prefix = 'C-Space'\n");
  config.reset(client_config_load());
  ASSERT_NE(config, nullptr);
  EXPECT_EQ(client_config_prefix(config.get()).ascii, ' ');
  EXPECT_EQ(client_config_prefix(config.get()).modifiers, KEYCODE_CONTROL);
  config.reset();

  save(temp.path.c_str(), "mux.keymap.prefix = nil\n");
  config.reset(client_config_load());
  ASSERT_NE(config, nullptr);
  EXPECT_EQ(client_config_prefix(config.get()).ascii, 0u);
  config.reset();

  save(temp.path.c_str(), "prefix = 'C-b'\n");
  config.reset(client_config_load());
  ASSERT_NE(config, nullptr);
  EXPECT_EQ(client_config_prefix(config.get()).ascii, ' ');
  EXPECT_EQ(client_config_prefix(config.get()).modifiers, KEYCODE_CONTROL);
  config.reset();

  save(temp.path.c_str(), "mux.keymap = 'bad'\n");
  EXPECT_EQ(client_config_load(), nullptr);
  save(temp.path.c_str(), "mux.keymap.prefix = 'bad'\n");
  EXPECT_EQ(client_config_load(), nullptr);
  save(temp.path.c_str(), "error('bad config')\n");
  EXPECT_EQ(client_config_load(), nullptr);

  save(temp.path.c_str(), "mux.keymap.prefix = 'C-Space'\n");
  config.reset(client_config_load());
  ASSERT_NE(config, nullptr);
  bool pending = false;
  auto process = [&](const char *key) {
    const char *input = key;
    size_t len = 1;
    bool forward = client_config_process(config.get(), &pending, &input, &len);
    EXPECT_EQ(len, 0u);
    EXPECT_EQ(input, key + 1);
    return forward;
  };
  EXPECT_FALSE(process("\x00"));
  EXPECT_TRUE(pending);
  EXPECT_TRUE(process("\x00"));
  EXPECT_FALSE(pending);
  EXPECT_FALSE(process("\x00"));
  EXPECT_TRUE(process("x")); // Unbound key passes through, unlike bound d.
  EXPECT_FALSE(pending);
  EXPECT_FALSE(process("\x00"));
  EXPECT_FALSE(process("d")); // Built-in prefix command is consumed.
  EXPECT_FALSE(pending);
  EXPECT_TRUE(process("y"));
}

}  // namespace
