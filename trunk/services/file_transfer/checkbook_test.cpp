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
#include <zlib.h>
#include "services/file_transfer/checkbook.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem/operations.hpp>
#include <sstream>
namespace {
static const char *kTestFile = "checkbooktest.1";
}

class CheckBookTest : public testing::Test {
 public:
  void CreateFile(int file_size, string *content = NULL) {
    boost::iostreams::mapped_file_params p(kTestFile);
    p.mode = std::ios_base::out | std::ios_base::trunc;
    p.new_file_size = file_size;
    boost::iostreams::mapped_file out;
    out.open(p);
    CHECK(out.is_open());
    for (int i = 0; i < file_size; ++i) {
      out.data()[i] = static_cast<char>(i);
      if (content != NULL) {
        content->push_back(static_cast<char>(i));
      }
    }
    out.close();
  }
};

TEST_F(CheckBookTest, Test1) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  CreateFile(kFileSize);
  scoped_ptr<CheckBook> checkbook(CheckBook::Create(
      "localhost", "1234", kTestFile, ""));
  EXPECT_TRUE(checkbook != NULL);
  ASSERT_EQ(checkbook->slice_size(), 2);
  ASSERT_EQ(checkbook->slice(0).offset(), 0);
  ASSERT_EQ(checkbook->slice(0).finished(), false);
  uint32 adler = adler32(0L, Z_NULL, 0);
  int k = 0;
  for (int i = 0; i < kFileSize ; ++i) {
    adler = adler32(adler, reinterpret_cast<const Bytef*>(&i), 1);
    if (i == CheckBook::GetSliceSize() - 1 || i == kFileSize - 1) {
      ASSERT_EQ(checkbook->slice(k++).adler(), adler);
    }
  }
  ASSERT_EQ(k, 2);
  boost::filesystem::remove(kTestFile);
}

TEST_F(CheckBookTest, Test2) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  string content;
  CreateFile(kFileSize, &content);
  scoped_ptr<CheckBook> checkbook(CheckBook::Create(
      "localhost", "1234", kTestFile, "111"));
  EXPECT_TRUE(checkbook != NULL);
  ASSERT_EQ(checkbook->slice_size(), 2);
  ASSERT_EQ(checkbook->slice(0).offset(), 0);
  ASSERT_EQ(checkbook->slice(0).length(), CheckBook::GetSliceSize());
  ASSERT_EQ(checkbook->slice(0).finished(), false);
  ASSERT_EQ(checkbook->slice(1).finished(), false);
  ASSERT_EQ(checkbook->slice(1).length(), 1);
  uint32 adler = adler32(0L, Z_NULL, 0);
  uint32 slice0_adler = adler32(
      adler,
      reinterpret_cast<const Bytef*>(content.c_str()),
      CheckBook::GetSliceSize());
  ASSERT_EQ(checkbook->slice(0).adler(), slice0_adler);
  ASSERT_EQ(checkbook->slice(0).previous_adler(), adler);


  int k = 0;
  for (int i = 0; i < kFileSize ; ++i) {
    adler = adler32(adler, reinterpret_cast<const Bytef*>(&i), 1);
    if (i == CheckBook::GetSliceSize() - 1 || i == kFileSize - 1) {
      ASSERT_EQ(checkbook->slice(k).checkbook_dest_filename(),
                checkbook->meta().checkbook_dest_filename());
      ASSERT_EQ(checkbook->slice(k++).adler(), adler);
    }
  }
  ASSERT_EQ(k, 2);
  checkbook->mutable_slice(1)->set_finished(true);
  VLOG(0) << "src file: " << checkbook->GetCheckBookDestFileName();
  VLOG(0) << "src file: " << checkbook->GetCheckBookDestFileName();
  VLOG(0) << "src file: " << checkbook->GetCheckBookDestFileName();
  ASSERT_TRUE(checkbook->Save(checkbook->GetCheckBookSrcFileName()));
  ASSERT_TRUE(checkbook->Save(checkbook->GetCheckBookSrcFileName()));
  ASSERT_TRUE(checkbook->Save(checkbook->GetCheckBookSrcFileName()));
  scoped_ptr<CheckBook> checkbook2(CheckBook::Create(
      "localhost", "1234", kTestFile, "111"));
  ASSERT_EQ(checkbook->GetCheckBookDestFileName(),
            checkbook2->GetCheckBookDestFileName());
  ASSERT_EQ(checkbook2->slice(1).finished(), true);
  scoped_ptr<CheckBook> checkbook3(CheckBook::Create(
      "localhost", "1234", kTestFile, "1112"));
  ASSERT_EQ(checkbook3->slice(1).finished(), false);
  checkbook2->Save(checkbook2->GetCheckBookDestFileName());
  checkbook2->Save(checkbook2->GetCheckBookDestFileName());
  VLOG(2) << "Dest: " << checkbook2->GetCheckBookDestFileName();
  scoped_ptr<CheckBook> checkbook4(CheckBook::Load(checkbook2->GetCheckBookDestFileName()));
  ASSERT_EQ(checkbook4->slice(1).finished(), true);
  boost::filesystem::remove(checkbook->GetCheckBookDestFileName());
  boost::filesystem::remove(checkbook2->GetCheckBookSrcFileName());
  boost::filesystem::remove(checkbook3->GetCheckBookSrcFileName());
  boost::filesystem::remove(kTestFile);
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
