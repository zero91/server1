#include "services/file_transfer/file_transfer.hpp"
FileTransfer::FileTransfer() {
}

void FileTransfer::Start() {
  if (!init_) {
    Init();
  }
}

void FileTransfer::Create(const string &host, const string &port, const string &filename, int threads) {
  
}
