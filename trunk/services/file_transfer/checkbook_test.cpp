// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

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
