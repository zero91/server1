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
#include "net/mac_address.hpp"
#include "base/stringprintf.hpp"
#include <sstream>
DEFINE_string(mac_address, "00:21:70:80:e4:37",
              "The mac address of the machine to run the test");

class MacAddressTest : public testing::Test {
 public:
};

TEST_F(MacAddressTest, Test1) {
  for (int i = 0; i < 10000; ++i) {
    string mac = GetMacAddress();
    ASSERT_EQ(mac, FLAGS_mac_address);
  }
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
