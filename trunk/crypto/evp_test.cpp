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



#include "base/base.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <glog/logging.h>
#include "crypto/evp.hpp"
#include <sstream>
const char *md5hello = "5d41402abc4b2a76b9719d911017c592";
class EvpTest : public testing::Test {
 public:
};

TEST_F(EvpTest, Test1) {
  scoped_ptr<EVP> evp(EVP::CreateMD5());
  EXPECT_TRUE(evp.get() != NULL);
  evp->Update("hello");
  evp->Finish();
  int i = evp->digest<int>();
  string is = evp->digest<string>();
  evp->Reset();
  evp->Update("h");
  evp->Update("e");
  evp->Update("l");
  evp->Update("lo");
  evp->Finish();
  int j = evp->digest<int>();
  string js = evp->digest<string>();
  ASSERT_EQ(is, js);
  ASSERT_EQ(js, md5hello);
  ASSERT_EQ(j, i);
  char buffer[128] = { 0 };
  snprintf(buffer, sizeof(buffer), "%x%x%x%x%x%x%x%x",
           (i >> 28) & 0xF, (i >> 24) & 0xF,
           (i >> 20) & 0xF, (i >> 16) & 0xF,
           (i >> 12) & 0xF, (i >> 8) & 0xF,
           (i >> 4) & 0xF, (i >> 0) & 0xF);
  ASSERT_EQ(buffer, js.substr(0, 8));
}

TEST_F(EvpTest, Test2) {
  scoped_ptr<EVP> evp(EVP::CreateMD5());
  EXPECT_TRUE(evp.get() != NULL);
  evp->Update("00:21:70:80:e4:37");
  evp->Update("localhost");
  evp->Update("1234");
  evp->Update("checkbooktest.1");
  evp->Update("1112");
  evp->Finish();
  string is = evp->digest<string>();
  VLOG(2) << is;
  ASSERT_EQ(is.size(), strlen(md5hello));
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
