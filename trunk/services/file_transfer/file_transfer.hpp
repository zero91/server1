#ifndef FILE_TRANSFER_HPP_
#define FILE_TRANSFER_HPP_
class FileTransfer {
 public:
  FileTransfer(const string &file_name, int threads);
  void Start();
  void Stop();
  void Suspend(const string &checkpoint);
  void Resume(const string &checkpoint);
  // The percent * 1000, 1000 means transfer finished.
  int Percent();
 private:
  ThreadPool pool_;
  IOService io_service_;
  vector<boost::shared_ptr<ClientConnection> > connections_;
  vector<shared_ptr<FileTransferService::Stub> > stubs_;
};
#endif  // FILE_TRANSFER_HPP_
