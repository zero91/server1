#ifndef FILE_TRANSFER_CLIENT_HPP_
#define FILE_TRANSFER_CLIENT_HPP_
#include "base/base.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "thread/threadpool.hpp"
#include "server/protobuf_connection.hpp"
#include "services/file_transfer/checkbook.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
class TransferTask;
class SliceStatus;
class FileTransferClient {
 public:
  void PushChannel(FullDualChannel *channel);
  void Start();
  void Stop();
  void set_finish_listener(const boost::function0<void> h) {
    finish_handler_ = h;
  }
  bool finished() const {
    return finished_;
  }
  static FileTransferClient *Create(
      const string &host, const string &port,
      const string &src_filename,
      const string &dest_filename,
      int threadpool_size);
  // The percent * 1000, 1000 means transfer finished.
  int Percent();
 private:
  FileTransferClient(int thread_pool_size) :
    pool_("FileTransferClientThreadPool", thread_pool_size),
    sync_checkbook_failed_(0), finished_(false) {
  }
  void Schedule();
  void SyncCheckBook();
  void SyncCheckBookDone(boost::shared_ptr<TransferTask> tasker, bool succeed);
  void ScheduleSlice();
  void SyncSlice(boost::shared_ptr<SliceStatus> slice,
                 boost::shared_ptr<TransferTask> tasker);
  void SyncSliceDone(boost::shared_ptr<TransferTask> tasker);
  static const int kSyncCheckBookRetry = 3;
  enum Status {
    SYNC_CHECKBOOK,
    SYNC_SLICE
  };
  typedef deque<boost::shared_ptr<TransferTask> > TransferTaskQueue;
  typedef list<boost::shared_ptr<SliceStatus> > SliceStatusLink;
  ThreadPool pool_;
  boost::function0<void> finish_handler_;
  PCQueue<boost::shared_ptr<TransferTask> > transfer_task_queue_;
  boost::mutex transfer_task_list_mutex_;
  list<boost::shared_ptr<TransferTask> > transfer_task_list_;
  scoped_ptr<CheckBook> checkbook_;
  boost::iostreams::mapped_file_source src_file_;
  SliceStatusLink transfering_slice_;
  Status status_;
  int sync_checkbook_failed_;
  bool finished_;
  friend class TransferTask;
};
#endif  // FILE_TRANSFER_CLIENT_HPP_
