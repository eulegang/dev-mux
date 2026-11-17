#include <gtest/gtest.h>

extern "C" {
#include "client/keycode.h"
}

namespace {

TEST(Keycode, PrintableAscii) {
  for (int ascii = 32; ascii <= 126; ++ascii) {
    SCOPED_TRACE("iteration " + std::to_string(ascii));
    const char input[] = {static_cast<char>(ascii), '\0'};
    keycode key = {};
    ASSERT_EQ(parse_keycode(input, &key), 0) << ascii;
    EXPECT_EQ(key.ascii, ascii) << ascii;
    EXPECT_EQ(key.modifiers, 0) << ascii;
  }
}

TEST(Keycode, ModifiersAndSpace) {
  struct Case {
    const char *input;
    unsigned char ascii;
    unsigned char modifiers;
  };
  const Case cases[] = {
      {"C-b", 'b', KEYCODE_CONTROL},
      {"c-b", 'b', KEYCODE_CONTROL},
      {"M-x", 'x', KEYCODE_OPTION},
      {"m-x", 'x', KEYCODE_OPTION},
      {"CM-@", '@', KEYCODE_CONTROL | KEYCODE_OPTION},
      {"cm-space", ' ', KEYCODE_CONTROL | KEYCODE_OPTION},
      {"MC-Space", ' ', KEYCODE_CONTROL | KEYCODE_OPTION},
      {"Space", ' ', 0},
      {"space", ' ', 0},
      {"cm- ", ' ', KEYCODE_CONTROL | KEYCODE_OPTION},
  };
  for (const auto &test : cases) {
    SCOPED_TRACE(test.input);
    keycode key = {};
    ASSERT_EQ(parse_keycode(test.input, &key), 0);
    EXPECT_EQ(key.ascii, test.ascii);
    EXPECT_EQ(key.modifiers, test.modifiers);
  }
}

TEST(Keycode, RejectsInvalidDescriptionsWithoutChangingResult) {
  const char *invalid[] = {
      "",    "C-",  "M-",    "CC-a", "MM-a", "Cc-a", "Mm-a",
      "bad", "C-ñ", "C-Tab", "\n",   "\x7f", "CM-",
  };
  for (const char *input : invalid) {
    SCOPED_TRACE(input);
    keycode key = {'z', KEYCODE_OPTION};
    EXPECT_EQ(parse_keycode(input, &key), -1);
    EXPECT_EQ(key.ascii, 'z');
    EXPECT_EQ(key.modifiers, KEYCODE_OPTION);
  }
}

TEST(Keycode, ParsingBasicInput) {
  const char *s = "ab c";
  size_t len = 4;
  const char *it = s;
  keycode code = {0, 0};

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(0, code.modifiers);
  EXPECT_EQ('a', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(0, code.modifiers);
  EXPECT_EQ('b', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(0, code.modifiers);
  EXPECT_EQ(' ', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(0, code.modifiers);
  EXPECT_EQ('c', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), -1);
}

TEST(Keycode, ParsingBasicControlInput) {
  const char *s = "\x01"
                  "b"
                  "\x00"
                  "\x06"
                  "\x1a";
  const char *it = s;
  size_t len = 5;
  keycode code = {0, 0};

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(KEYCODE_CONTROL, code.modifiers);
  EXPECT_EQ('a', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(0, code.modifiers);
  EXPECT_EQ('b', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(KEYCODE_CONTROL, code.modifiers);
  EXPECT_EQ(' ', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(KEYCODE_CONTROL, code.modifiers);
  EXPECT_EQ('f', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(KEYCODE_CONTROL, code.modifiers);
  EXPECT_EQ('z', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), -1);
}

TEST(Keycode, ParsingBasicOptInput) {
  const char *s = "\x1b"
                  "b"
                  "\x1b"
                  "c";
  const char *it = s;
  size_t len = 4;
  keycode code = {0, 0};

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(KEYCODE_OPTION, code.modifiers);
  EXPECT_EQ('b', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(KEYCODE_OPTION, code.modifiers);
  EXPECT_EQ('c', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), -1);
}

TEST(Keycode, ParsingCSIInput) {
  const char s[] = "\e[97;1u"
                   "\e[103;2u"
                   "\e[103;5u";
  const char *it = s;
  size_t len = sizeof(s) - 1;

  keycode code = {0, 0};

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(0, code.modifiers);
  EXPECT_EQ('a', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(0, code.modifiers);
  EXPECT_EQ('G', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), 0);
  EXPECT_EQ(KEYCODE_CONTROL, code.modifiers);
  EXPECT_EQ('g', code.ascii);

  ASSERT_EQ(parse_keycode_input(&len, &it, &code), -1);
}
} // namespace
