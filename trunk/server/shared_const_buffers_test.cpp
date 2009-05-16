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
