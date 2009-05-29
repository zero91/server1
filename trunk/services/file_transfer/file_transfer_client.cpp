#include "services/file_transfer/file_transfer_client.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include <boost/filesystem/operations.hpp>
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

class TransferTask : public boost::enable_shared_from_this<TransferTask> {
 public:
  TransferTask(
      FileTransferClient *file_transfer,
      FullDualChannel *channel,
      const string &host, const string &port, int id);
  int id() const {
    return id_;
  }
  void set_status(boost::shared_ptr<SliceStatus> status) {
    status_ = status;
  }
  void SyncSlice();
  void SyncCheckBook(const FileTransfer::CheckBook *checkbook);
  FileTransfer::SliceRequest *mutable_request() {
    return &slice_request_;
  }
 private:
  void SyncCheckBookDone();
  void SyncSliceDone();
  static const int kRetry = 2;
  FileTransfer::FileTransferService::Stub stub_;
  FileTransfer::SliceRequest slice_request_;
  FileTransfer::SliceResponse slice_response_;
  FileTransfer::CheckBookResponse checkbook_response_;
  RpcController controller_;
  FileTransferClient *file_transfer_;
  boost::shared_ptr<SliceStatus> status_;
  int id_;
};

void FileTransferClient::PushChannel(FullDualChannel *channel) {
  static int id = 0;
  boost::shared_ptr<TransferTask> tasker(new TransferTask(
          this,
          channel,
          checkbook_->meta().host(),
          checkbook_->meta().port(), id++));
  {
    boost::mutex::scoped_lock locker(transfer_task_list_mutex_);
    transfer_task_list_.push_back(tasker);
  }
  transfer_task_queue_.Push(tasker);
}

void FileTransferClient::Start() {
  if (pool_.IsRunning()) {
    LOG(WARNING) << "FileTransferClient is running";
    return;
  }
  pool_.Start();
  if (checkbook_->meta().synced_with_dest()) {
    status_ = SYNC_SLICE;
  } else {
    status_ = SYNC_CHECKBOOK;
  }
  pool_.PushTask(boost::bind(
      &FileTransferClient::Schedule, this));
}

void FileTransferClient::Stop() {
  boost::shared_ptr<TransferTask> tasker;
  transfer_task_queue_.Push(tasker);
  pool_.Stop();
  if (!finished()) {
    checkbook_->Save(checkbook_->GetCheckBookSrcFileName());
  }
}

FileTransferClient *FileTransferClient::Create(
    const string &host, const string &port,
    const string &src_filename,
    const string &dest_filename,
    int threadpool_size) {
  FileTransferClient *file_transfer = new FileTransferClient(threadpool_size);
  file_transfer->checkbook_.reset(
      CheckBook::Create(host, port, src_filename, dest_filename));
  if (!file_transfer->checkbook_.get()) {
    delete file_transfer;
    return NULL;
  }
  return file_transfer;
}

int FileTransferClient::Percent() {
  int cnt = 0;
  for (int i = 0; i < checkbook_->slice_size(); ++i) {
    if (checkbook_->slice(i).finished()) {
      ++cnt;
    }
  }
  VLOG(2) << "Cnt: " << cnt;
  return cnt * 1000 / checkbook_->slice_size();
}

void FileTransferClient::Schedule() {
  switch (status_) {
    case SYNC_CHECKBOOK:
      if (sync_checkbook_failed_ < kSyncCheckBookRetry) {
        SyncCheckBook();
      } else {
        LOG(WARNING) << "Sync checkbook failed after retry "
                     << sync_checkbook_failed_;
      }
      return;
    case SYNC_SLICE:
      ScheduleSlice();
      return;
  }
}

void FileTransferClient::SyncCheckBook() {
  boost::shared_ptr<TransferTask> tasker = transfer_task_queue_.Pop();
  tasker->SyncCheckBook(checkbook_.get());
}

void TransferTask::SyncCheckBook(
    const FileTransfer::CheckBook *checkbook) {
  stub_.ReceiveCheckBook(&controller_,
                         checkbook,
                         &checkbook_response_,
                         NewClosure(boost::bind(
                             &TransferTask::SyncCheckBookDone,
                             this)));
}

void TransferTask::SyncCheckBookDone() {
  if (controller_.Failed()) {
    VLOG(2) << "transfer id: " << id_ << " sync checkbook failed";
    controller_.Reset();
    file_transfer_->SyncCheckBookDone(shared_from_this(), false);
  } else {
    file_transfer_->SyncCheckBookDone(shared_from_this(), true);
  }
}

void FileTransferClient::SyncCheckBookDone(
    boost::shared_ptr<TransferTask> tasker,
    bool succeed) {
  if (succeed) {
    status_ = SYNC_SLICE;
    checkbook_->mutable_meta()->set_synced_with_dest(true);
    checkbook_->Save(checkbook_->GetCheckBookSrcFileName());
    transfer_task_queue_.Push(tasker);
  } else {
    ++sync_checkbook_failed_;
  }
  pool_.PushTask(boost::bind(
      &FileTransferClient::Schedule, this));
  return;
}

void FileTransferClient::ScheduleSlice() {
  // Open the source file.
  const FileTransfer::MetaData &meta = checkbook_->meta();
  src_file_.open(meta.src_filename());
  if (!src_file_.is_open()) {
    LOG(WARNING) << "Fail to open source file: "
                 << meta.src_filename();
    return;
  }

  for (int i = 0; i < checkbook_->slice_size(); ++i) {
    if (checkbook_->slice(i).finished()) {
      continue;
    }
    boost::shared_ptr<SliceStatus> slice_status(
        new SliceStatus(checkbook_->slice(i).index()));
    transfering_slice_.push_back(slice_status);
  }

  for (;;) {
    boost::shared_ptr<TransferTask> tasker = transfer_task_queue_.Pop();
    if (tasker.get() == NULL) {
      VLOG(2) << "ScheduleSlice get NULL tasker, exit.";
      return;
    }
    boost::shared_ptr<SliceStatus> slice;
    bool in_transfering = false;
    for (SliceStatusLink::iterator it = transfering_slice_.begin();
         it != transfering_slice_.end();) {
      SliceStatusLink::iterator next = it;
      ++next;
      boost::shared_ptr<SliceStatus> local_slice = *it;
      if (local_slice->status() == SliceStatus::DONE) {
        VLOG(2) << "slice " << local_slice->index() << " Done";
        checkbook_->mutable_slice(local_slice->index())->set_finished(true);
        transfering_slice_.erase(it);
        it = next;
        continue;
      } else if (local_slice->status() == SliceStatus::TRANSFERING) {
        VLOG(2) << "slice " << local_slice->index() << " Transfering";
        it = next;
        in_transfering = true;
        continue;
      } else {
        slice = *it;
        slice->set_status(SliceStatus::TRANSFERING);
        break;
      }
    }
    if (slice.get() == NULL && !in_transfering) {
      VLOG(2) << "Transfer success!";
      finished_ = true;
      boost::filesystem::remove(checkbook_->GetCheckBookSrcFileName());
      if (!finish_handler_.empty()) {
        finish_handler_();
      }
      return;
    }
    if (slice.get() == NULL) {
      VLOG(2) << "Get null slice, retry";
      transfer_task_queue_.Push(tasker);
      continue;
    }
    VLOG(2) << "Get task: " << tasker->id();
    pool_.PushTask(boost::bind(
        &FileTransferClient::SyncSlice, this,
        slice, tasker));
  }
}

void FileTransferClient::SyncSlice(
    boost::shared_ptr<SliceStatus> slice,
    boost::shared_ptr<TransferTask> tasker) {
  FileTransfer::SliceRequest *request = tasker->mutable_request();
  request->Clear();
  tasker->set_status(slice);
  const int index = slice->index();
  const FileTransfer::Slice &slice_meta = checkbook_->slice(index);
  const int length = slice_meta.length();
  const int offset = slice_meta.offset();
  request->mutable_slice()->CopyFrom(slice_meta);
  request->mutable_content()->assign(
      src_file_.data() + offset, length);
  tasker->SyncSlice();
}

void TransferTask::SyncSlice() {
  status_->set_status(SliceStatus::TRANSFERING);
  stub_.ReceiveSlice(&controller_,
                     &slice_request_,
                     &slice_response_,
                     NewClosure(boost::bind(
                         &TransferTask::SyncSliceDone,
                         this)));
}

void TransferTask::SyncSliceDone() {
  if (controller_.Failed()) {
    VLOG(2) << "transfer id: " << id_ << " slice: "
            << slice_request_.slice().offset()
            << " Failed";
    // Retry.
    controller_.Reset();
    status_->set_status(SliceStatus::IDLE);
  } else {
    status_->set_status(SliceStatus::DONE);
  }
  VLOG(2) << "SyncSlice: " << status_->index() << " Done";
  file_transfer_->SyncSliceDone(shared_from_this());
}

void FileTransferClient::SyncSliceDone(
    boost::shared_ptr<TransferTask> tasker) {
  transfer_task_queue_.Push(tasker);
  VLOG(2) << "SyncSlice Done";
}

TransferTask::TransferTask(
    FileTransferClient *file_transfer,
    FullDualChannel *channel,
    const string &host, const string &port, int id)
    : stub_(channel), id_(id), file_transfer_(file_transfer) {
}
