#include "services/file_transfer/checkbook.hpp"
#include "base/stringprintf.hpp"
#include "net/mac_address.hpp"
#include "crypto/evp.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <glog/logging.h>
#include <zlib.h>
#include <errno.h>
#include <string.h>
CheckBook *CheckBook::Create(
    const string &host,
    const string &port,
    const string &src_filename,
    const string &dest_filename) {
  CheckBook *checkbook;
  checkbook = Load(InternalGetCheckBookSrcFileName(
      host, port, src_filename, dest_filename));
  if (checkbook != NULL) {
    return checkbook;
  }

  checkbook = new CheckBook;
  FileTransfer::MetaData *meta = checkbook->mutable_meta();
  meta->set_host(host);
  meta->set_port(port);
  meta->set_src_filename(src_filename);
  meta->set_dest_filename(dest_filename);
  meta->set_synced_with_dest(false);
  string mac_address = GetMacAddress();
  meta->set_src_mac_address(mac_address);
  boost::filesystem::path p(src_filename, boost::filesystem::native);
  if (!boost::filesystem::exists(p)) {
    LOG(WARNING) << "Not find: " << src_filename;
    delete checkbook;
    return NULL;
  }
  if (!boost::filesystem::is_regular(p)) {
    LOG(WARNING) << "Not a regular file: " << src_filename;
    delete checkbook;
    return NULL;
  }
  boost::iostreams::mapped_file_source file;
  file.open(src_filename);
  if (!file.is_open()) {
    LOG(WARNING) << "Fail to open file: " << src_filename;
    delete checkbook;
    return NULL;
  }
  const Bytef *data = reinterpret_cast<const Bytef*>(file.data());
  int file_size = file.size();
  int slice_number = (file_size + kSliceSize - 1) / kSliceSize;
  VLOG(2) << src_filename << " size: " << file_size
          << " slice_size: " << kSliceSize
          << " slice_number: " << slice_number;
  const int odd = file_size - (slice_number - 1) * kSliceSize;
  uint32 adler = adler32(0L, Z_NULL, 0);
  uint32 previous_adler = adler;
  const string checkbook_dest_filename = checkbook->GetCheckBookDestFileName();
  meta->set_checkbook_dest_filename(checkbook_dest_filename);
  for (int i = 0; i < slice_number; ++i) {
    FileTransfer::Slice *slice = checkbook->add_slice();
    slice->set_index(i);
    slice->set_offset(i * kSliceSize);
    slice->set_finished(false);
    slice->set_checkbook_dest_filename(checkbook_dest_filename);
    int length = kSliceSize;
    if (i == slice_number - 1) {
      length = odd;
    }
    slice->set_length(length);
    previous_adler = adler;
    adler = adler32(adler, data, length);
    slice->set_previous_adler(previous_adler);
    slice->set_adler(adler);
    VLOG(4) << "slice " << i << " previous_adler: " << slice->previous_adler() << " adler: " << slice->adler() << " length: " << slice->length();
  }
  return checkbook;
}

string CheckBook::GetCheckBookDestFileName(const FileTransfer::MetaData *meta) {
  scoped_ptr<EVP> evp(EVP::CreateMD5());
  evp->Update(meta->src_mac_address());
  evp->Update(meta->host());
  evp->Update(meta->port());
  evp->Update(meta->src_filename());
  evp->Update(meta->dest_filename());
  evp->Finish();
  const string suffix = evp->digest<string>();
  return meta->dest_filename() + "." + suffix;
}

string CheckBook::InternalGetCheckBookSrcFileName(
      const string &host, const string &port,
      const string &src_filename,
      const string &dest_filename) {
  scoped_ptr<EVP> evp(EVP::CreateMD5());
  evp->Update(host);
  evp->Update(port);
  evp->Update(src_filename);
  evp->Update(dest_filename);
  evp->Finish();
  return src_filename + "." + evp->digest<string>();
}

string CheckBook::GetCheckBookSrcFileName() const {
  const FileTransfer::MetaData &meta = this->meta();
  return InternalGetCheckBookSrcFileName(
      meta.host(), meta.port(), meta.src_filename(), meta.dest_filename());
}

CheckBook *CheckBook::Load(const string &checkbook_filename) {
  if (!boost::filesystem::exists(checkbook_filename)) {
    return NULL;
  }
  CheckBook *checkbook = new CheckBook;
  ifstream input(checkbook_filename.c_str(), ios::in | ios::binary);
  if (!checkbook->ParseFromIstream(&input)) {
    LOG(WARNING) << "Fail to parse checkbook: " << checkbook_filename;
    return NULL;
  }
  return checkbook;
}

bool CheckBook::Save(const FileTransfer::CheckBook *checkbook,
                     const string &filename) {
  ofstream output(filename.c_str(), ios::out | ios::trunc | ios::binary);
  if (!checkbook->SerializeToOstream(&output)) {
    LOG(WARNING) << "Failed to save the CheckBook to:" << filename
                 << " error: " << strerror(errno);
    return false;
  }
  VLOG(2) << "Save checkbook: " << filename;
  return true;
}
