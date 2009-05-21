#include "services/file_transfer/checkbook.hpp"
CheckBook *CheckBook::Create(
    const string &host,
    const string &port,
    const string &filename) {
  scoped_ptr<CheckBook> checkbook = new CheckBook;
  FileTransfer::MetaData *meta = checkbook->mutable_meta();
  meta->set_host(host);
  meta->set_port(port);
  meta->set_filename(filename);
  boost::filesystem::path p(filename, boost::filesystem::native);
  if (!boost::filesystem::exists(p)) {
    LOG(WARNING) << "Not find: " << filename;
    delete checkbook;
    return NULL;
  }
  if (!boost::filesystem::is_regular(p)) {
    LOG(WARNING) << "Not a regular file: " << filename;
    delete checkbook;
    return NULL;
  }
  uint32 adler = adler32(0L, Z_NULL, 0);
  boost::iostreams::mapped_file_source file;
  if (!file.open(filename)) {
    LOG(WARNING) << "Fail to open file: " << filename;
    delete checkbook;
    return NULL;
  }
  const char *data = file.data();
  int file_size = file.size();
  int slice_number = (file_size + kSliceSize - 1) / kSliceSize;
  const int odd = file_size - (slice_number - 1) * kSliceSize;
  uint32 adler = adler32(0L, Z_NULL, 0);
  uint32 previous_adler = adler;
  for (int i = 0; i < slice_number; ++i) {
    FileTransfer::Slice *slice = checkbook->add_slice();
    slice->set_index(i);
    int length = kSliceSize;
    if (i == slice_number - 1) {
      length = odd;
      slice->set_length(length);
    }
    previous_adler = adler;
    adler = adler32(adler, data, length);
    slice->set_adler(adler);
  }
  return checkbook_;
}

CheckBook *CheckBook::Load(const string &filename) {
  CheckBook *checkbook = new CheckBook;
  fstream input(filename, ios::in | ios::binary);
  if (!check_book->ParseFromIstream(&input)) {
    LOG(WARNING) << "Fail to parse checkbook: " << filename;
    return NULL;
  }
}

void CheckBook::Save(const string &filename) {
  fstream output(filename, ios::out | ios::trunc | ios::binary);
  if (SerializeToOstream(&output)) {
    LOG(WARNING) << "Failed to save the CheckBook";
    return;
  }
  return;
}
