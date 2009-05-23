#include <string.h>
#include <stdio.h>
#include <boost/iostreams/device/mapped_file.hpp>
#include <string>
int main(int argc, char *argv[])
{
  boost::iostreams::mapped_file_source file;
  file.open(argv[1]);
  if (!file.is_open()) {
    printf("fail to open:%s\n", argv[1]);
    return -1;
  }
  const char *data = file.data();
  std::string s(file.data(), file.size());
  printf("%s\n", s.c_str());
  boost::iostreams::mapped_file_params p(std::string(argv[1]) + ".out");
  p.mode = std::ios_base::out;
  p.new_file_size = file.size();
  boost::iostreams::mapped_file out;
  out.open(p);
  for (int i = 0; i < file.size(); ++i) {
    out.data()[i] = data[i];
  }
  return 0;
}
