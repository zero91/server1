// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <glog/logging.h>
#include "base/stringprintf.hpp"
namespace {

TEST(StringPrintfTest, Empty) {
  EXPECT_EQ("", StringPrintf(string().c_str()));
  EXPECT_EQ("", StringPrintf("%s", ""));
}

TEST(StringPrintfTest, Misc) {
  EXPECT_EQ("123hello w", StringPrintf("%3$d%2$s %1$c", 'w', "hello", 123));
}

TEST(StringAppendFTest, Empty) {
  string value("Hello");
  const char* empty = "";
  StringAppendF(&value, empty);
  EXPECT_EQ("Hello", value);
}

TEST(StringAppendFTest, EmptyString) {
  string value("Hello");
  StringAppendF(&value, "%s", "");
  EXPECT_EQ("Hello", value);
}

TEST(StringAppendFTest, String) {
  string value("Hello");
  StringAppendF(&value, " %s", "World");
  EXPECT_EQ("Hello World", value);
}

TEST(StringAppendFTest, Int) {
  string value("Hello");
  StringAppendF(&value, " %d", 123);
  EXPECT_EQ("Hello 123", value);
}

}  // namespace

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
