#ifndef CHECKBOOK_HPP_
#define CHECKBOOK_HPP_
#include "services/file_transfer/checkbook.pb.h"
class CheckBook : public FileTransfer::CheckBook {
 public:
  CheckBook *Create(const string &host, const string &port,
            const string &filename);
  CheckBook *Load(const string &filename);
  void Save(const string &filenaem);
 private:
  CheckBook();
};
#endif  // CHECKBOOK_HPP_
