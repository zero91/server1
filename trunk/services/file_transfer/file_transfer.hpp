#ifndef FILE_TRANSFER_HPP_
#define FILE_TRANSFER_HPP_
class FileTransfer {
 public:
  void Start();
  void Stop();
  void Suspend(const string &checkbook);
  FileTransfer *Resume(const string &checkbook);
  FileTransfer *Create(const string &host, const string &port, const string &filename, int threads)
  // The percent * 1000, 1000 means transfer finished.
  int Percent();
 private:
  FileTransfer();
  ThreadPool pool_;
  IOService io_service_;
  vector<boost::shared_ptr<ClientConnection> > connections_;
  vector<shared_ptr<FileTransferService::Stub> > stubs_;
  CheckBook check_book_;
  bool init_;
};
#endif  // FILE_TRANSFER_HPP_
