/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */



#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include "server/shared_const_buffers.hpp"
class SharedConstBuffersTest : public testing::Test {
 public:
 protected:
  string GetP(const SharedConstBuffers &p) {
    string ret;
    for (SharedConstBuffers::const_iterator it = p.begin();
         it != p.end(); ++it) {
      ret.append(string(boost::asio::buffer_cast<const char *>(*it), boost::asio::buffer_size(*it)));
    }
    return ret;
  }
  static const int kPoolSize = 100;
};

TEST_F(SharedConstBuffersTest, Test1) {
  SharedConstBuffers p;
  p.push(new string("hello"));
  p.push(new string("world"));
  SharedConstBuffers p1 = p;
  p.consume(1);
  EXPECT_EQ(GetP(p), "elloworld");
  p.consume(1);
  EXPECT_EQ(GetP(p),  "lloworld");
  p.consume(2);
  EXPECT_EQ(GetP(p),    "oworld");
  p.consume(2);
  EXPECT_EQ(GetP(p),      "orld");
  p.consume(2);
  EXPECT_EQ(GetP(p),        "ld");
  p.consume(2);
  EXPECT_EQ(GetP(p), "");
  EXPECT_TRUE(p.empty());
  EXPECT_TRUE(p.begin() == p.end());
  p.clear();
  EXPECT_EQ(GetP(p1), "helloworld");
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
