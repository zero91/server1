#ifndef CHECKBOOK_HPP_
#define CHECKBOOK_HPP_
#include "base/base.hpp"
#include "services/file_transfer/checkbook.pb.h"
class CheckBook : public FileTransfer::CheckBook {
 public:
  // New a checkbook or load the existing checkbook.
  static CheckBook *Create(
      const string &host, const string &port, const string &src_filename, const string &dest_filename);
  static CheckBook *Load(const string &checkbook_filename);
  bool Save(const string &filename);
  static const int kSliceSize = 640 * 1024;
  string GetCheckBookDestFileName() const;
  string GetCheckBookSrcFileName() const;
 private:
  static string InternalGetCheckBookSrcFileName(
      const string &host, const string &port,
      const string &src_filename,
      const string &dest_filename);

  static string GetEvpName(const string &host, const string &port,
                           const string &src_filename, const string &dest_filename);
  CheckBook() : FileTransfer::CheckBook() {
  }
};
#endif  // CHECKBOOK_HPP_
