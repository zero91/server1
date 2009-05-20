#include "services/file_transfer/file_transfer.hpp"
FileTransfer::FileTransfer() {
}

void FileTransfer::Start(int threads) {
}

FileTransfer *FileTransfer::Create(
    const string &host, const string &port,
    const string &filename, int threads) {
  FileTransfer::MetaData *meta = checkbook_.mutable_meta();
  meta->set_host(host);
  meta->set_port(port);
  meta->set_filename(filename);
  meta->set_threads(threads);
  boost::filesystem::path p(filename, boost::filesystem::native);
  if (!boost::filesystem::exists(p)) {
    LOG(WARNING) << "Not find: " << filename;
    return NULL;
  }
  if (!boost::filesystem::is_regular(p)) {
    LOG(WARNING) << "Not a regular file: " << filename;
    return NULL;
  }
  int file_size = boost::filesystem::file_size(p);
  int slice_number = (file_size + kSliceSize - 1) / kSliceSize;
  uint32 adler = adler32(0L, Z_NULL, 0);
  scoped_array<char> slice_buffer(new char[kSliceSize]);
  while (
}
