#ifndef FILE_TRANSFER_HPP_
#define FILE_TRANSFER_HPP_
class FileTransfer {
 private:
  class TransferTask : public boost::enable_shared_from_this<TransferTask> {
   public:
    TransferTask(
        FileTransfer *file_transfer,
        const string &host, const string &port, int id);
    ~TransferTask();
    int id() const {
      return id_;
    }
    void set_status(boost::shared_ptr<SliceStatus> status) {
      status_ = status;
    }

    void Transfer(const SliceRequest &request);
    void SyncCheckBook(const FileTransfer::CheckBook *checkbook);
    FileTransfer::SliceRequest *mutable_request() const {
      return &request_;
    }
   private:
    static const int kRetry = 2;
    ClientConnection connection_;
    FileTransfer::RecieveSliceService::Stub stub_;
    FileTransfer::SliceRequest request_;
    FileTransfer::SliceResponse response_;
    RpcController controller_;
    FileTransfer *file_transfer_;
    boost::shared_ptr<SliceStatus> status_;
    int id_;
  };

  class SliceStatus {
   public:
    enum Status {IDLE, TRANSFERING, DONE};
    SliceStatus(int index) : index_(index), status_(IDLE) {
    }
    int index() const {
      return index_;
    }
    Status status() const {
      return status_;
    }
    void set_status(Status status) {
      status_ = status;
    }
   private:
    mutable int index_;
    mutable Status status_;
  };

 public:
  void Start();
  void Stop();
  void set_finish_listener(const boost::function0<void> h) {
    finish_handler_ = h;
  }
  void Suspend(const string &checkbook);
  FileTransfer *Create(
      const string &host, const string &port,
      const string &src_filename,
      const string &dest_filename);
  // The percent * 1000, 1000 means transfer finished.
  int Percent();
 private:
  static const int kSyncCheckBookRetry = 3;
  enum Status {
    SYNC_CHECKBOOK,
    SYNC_SLICE
  };
  typedef deque<shared_ptr<TransferTask> > TransferTaskQueue;
  typedef list<boost::shared_ptr<SliceStatus> > SliceStatusLink;
  static const int kSliceSize = 1024 * 640;  // 640K per slices.
  FileTransfer();
  ThreadPool pool_;
  IOService io_service_;
  boost::function0<void> finish_handler_;
  PCQueue<shared_ptr<TransferTask> > transfer_task_queue_;
  list<shared_ptr<TransferTask> > transfer_task_list_;
  scoped_ptr<CheckBook> check_book_;
  boost::iostream::mapped_file_source src_file_;
  SliceStatusLink transfering_slice_;
};
#endif  // FILE_TRANSFER_HPP_
