#include "services/file_transfer/file_transfer_client.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/thread/shared_mutex.hpp>
class SliceStatus {
 public:
  enum Status {IDLE = 0, TRANSFERING, DONE};
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
  static boost::shared_ptr<TransferTask> Create(FileTransferClient *file_transfer,
      FullDualChannel *channel, int id);

  int id() const {
    return id_;
  }
  void set_status(boost::shared_ptr<SliceStatus> status) {
    status_ = status;
  }
  void SyncSlice();
  bool SyncCheckBook(const FileTransfer::CheckBook *checkbook);
  FileTransfer::SliceRequest *mutable_request() {
    return &slice_request_;
  }
  bool IsConnected() const {
    return !channel_closed_;
  }
 private:
  TransferTask(
      FileTransferClient *file_transfer,
      FullDualChannel *channel, int id);
  void SyncSliceDone();
  void ChannelClosed() {
    VLOG(2) << "ChannelClosed";
    channel_closed_ = true;
    file_transfer_->ChannelClosed(shared_from_this());
  }
  static const int kRetry = 2;
  FileTransfer::FileTransferService::Stub stub_;
  FileTransfer::SliceRequest slice_request_;
  FileTransfer::SliceResponse slice_response_;
  FileTransfer::CheckBookResponse checkbook_response_;
  RpcController controller_;
  FileTransferClient *file_transfer_;
  boost::shared_ptr<SliceStatus> status_;
  int id_;
  bool channel_closed_;
};

void FileTransferClient::ChannelClosed(boost::shared_ptr<TransferTask> tasker) {
  VLOG(2) << "FileTransferClient::ChannelClosed";
  boost::mutex::scoped_lock locker(transfer_task_set_mutex_);
  transfer_task_set_.erase(tasker);
}

void FileTransferClient::PushChannel(FullDualChannel *channel) {
  static int id = 0;
  if (!channel->IsConnected()) {
    LOG(WARNING) << "Channel is not connected";
    return;
  }
  boost::shared_ptr<TransferTask> tasker(TransferTask::Create(
          this,
          channel,
          id++));
  {
    boost::mutex::scoped_lock locker(transfer_task_set_mutex_);
    transfer_task_set_.insert(tasker);
  }
  transfer_task_queue_.Push(tasker);
  pool_.PushTask(boost::bind(
      &FileTransferClient::Schedule, this));
}

void FileTransferClient::Start() {
  if (pool_.IsRunning()) {
    LOG(WARNING) << "FileTransferClient is running";
    return;
  }
  pool_.Start();
}

void FileTransferClient::Stop() {
  boost::shared_ptr<TransferTask> tasker;
  transfer_task_queue_.Push(tasker);
  pool_.Stop();
  if (!finished()) {
    VLOG(1) << "SaveCheckBook to: " << checkbook_->GetCheckBookSrcFileName();
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
  VLOG(2) << "Schedule, status: " << status_;
  switch (status_) {
    case SYNC_CHECKBOOK:
      SyncCheckBook();
      return;
    case PREPARE_SLICE:
      PrepareSlice();
      return;
    case SYNC_SLICE:
      ScheduleSlice();
      return;
  }
}

void FileTransferClient::SyncCheckBook() {
  boost::mutex::scoped_lock locker(sync_checkbook_mutex_);
  if (status_ != SYNC_CHECKBOOK) {
    LOG(WARNING) << "SyncCheckBook but status is: " << status_;
    pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
    return;
  }
  if (sync_checkbook_failed_ >= kSyncCheckBookRetry) {
    LOG(WARNING) << "SyncCheckbook failed: " << sync_checkbook_failed_;
    return;
  }
  if (checkbook_->meta().synced_with_dest()) {
    LOG(WARNING) << "Already synced with dest";
    status_ = PREPARE_SLICE;
    pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
    return;
  }
  boost::shared_ptr<TransferTask> tasker = transfer_task_queue_.Pop();
  if (!tasker->SyncCheckBook(checkbook_.get())) {
    LOG(WARNING) << "Transfer checkbook failed, tasker: " << tasker->IsConnected();
    ++sync_checkbook_failed_;
    if (tasker->IsConnected()) {
      transfer_task_queue_.Push(tasker);
      if (pool_.IsRunning()) {
        pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
      } else {
        LOG(WARNING) << "Pool is stopped";
      }
    }
  } else {
    status_ = PREPARE_SLICE;
    checkbook_->mutable_meta()->set_synced_with_dest(true);
    checkbook_->Save(checkbook_->GetCheckBookSrcFileName());
    if (tasker->IsConnected()) {
      transfer_task_queue_.Push(tasker);
      if (pool_.IsRunning()) {
        pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
      } else {
        LOG(WARNING) << "Pool is stopped";
      }
    }
  }
}

bool TransferTask::SyncCheckBook(
    const FileTransfer::CheckBook *checkbook) {
  if (IsConnected()) {
    VLOG(1) << "SyncCheckBook";
    stub_.ReceiveCheckBook(&controller_,
                           checkbook,
                           &checkbook_response_,
                           NULL);
    controller_.Wait();
    if (controller_.Failed() || !checkbook_response_.succeed()) {
      VLOG(2) << "transfer id: " << id_ << " sync checkbook failed";
      controller_.Reset();
      return false;
    }
    return true;
  }
}

void FileTransferClient::PrepareSlice() {
  boost::mutex::scoped_lock locker(prepare_slice_mutex_);
  if (status_ != PREPARE_SLICE) {
    LOG(WARNING) << "PrepareSlice but status is: " << status_;
    pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
    return;
  }
  VLOG(1) << "PrepareSlice";
  // Open the source file.
  const FileTransfer::MetaData &meta = checkbook_->meta();
  src_file_.open(meta.src_filename());
  if (!src_file_.is_open()) {
    LOG(WARNING) << "Fail to open source file: "
                 << meta.src_filename();
    return;
  }

  {
    boost::mutex::scoped_lock locker(transfering_slice_mutex_);
    for (int i = 0; i < checkbook_->slice_size(); ++i) {
      if (checkbook_->slice(i).finished()) {
        continue;
      }
      boost::shared_ptr<SliceStatus> slice_status(
          new SliceStatus(checkbook_->slice(i).index()));
      transfering_slice_.push_back(slice_status);
    }
  }
  status_ = SYNC_SLICE;
  pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
}

void FileTransferClient::ScheduleSlice() {
  boost::shared_ptr<SliceStatus> slice;
  bool in_transfering = false;
  bool call_finish_handler = false;
  {
    boost::mutex::scoped_lock locker(transfering_slice_mutex_);
    VLOG(2) << "slice list size: " << transfering_slice_.size();
    if (status_ != SYNC_SLICE) {
      LOG(WARNING) << "ScheduleSlice but status is: " << status_;
      pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
      return;
    }
    for (SliceStatusLink::iterator it = transfering_slice_.begin();
         it != transfering_slice_.end();) {
      SliceStatusLink::iterator next = it;
      ++next;
      boost::shared_ptr<SliceStatus> local_slice = *it;
      if (local_slice->status() == SliceStatus::DONE) {
        VLOG(1) << "slice " << local_slice->index() << " Done";
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
        VLOG(2) << "Get slice: " << slice->index();
        break;
      }
    }
    if (slice.get() == NULL && !in_transfering && !finished_) {
      VLOG(1) << "Transfer success!";
      finished_ = true;
      call_finish_handler = true;
    }
  }
  if (call_finish_handler) {
    VLOG(0) << "Call finihsed handler, remove: " << checkbook_->GetCheckBookSrcFileName();
    boost::filesystem::remove(checkbook_->GetCheckBookSrcFileName());
    if (!finish_handler_.empty()) {
      finish_handler_();
    }
    return;
  }
  if (slice.get() == NULL) {
    VLOG(2) << "Get null slice, retry";
    pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
    return;
  }
  boost::shared_ptr<TransferTask> tasker = transfer_task_queue_.Pop();
  if (tasker.get() == NULL) {
    VLOG(2) << "ScheduleSlice get NULL tasker, exit.";
    return;
  }
  VLOG(2) << "Get tasker " << tasker->id();
  if (!tasker->IsConnected()) {
    VLOG(2) << "Task is not connected!";
    return;
  }
  VLOG(2) << "Get task: " << tasker->id();
  SyncSlice(slice, tasker);
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
  if (IsConnected()) {
    VLOG(1) << "SyncSlice: " << status_->index();
    stub_.ReceiveSlice(&controller_,
                       &slice_request_,
                       &slice_response_,
                       NewClosure(boost::bind(
                           &TransferTask::SyncSliceDone,
                           this)));
  } else {
    file_transfer_->SyncSliceDone(shared_from_this(), false, status_);
  }
}

void TransferTask::SyncSliceDone() {
  bool ret = false;
  if (controller_.Failed() || !slice_response_.succeed()) {
    VLOG(2) << "transfer id: " << id_ << " slice: "
            << slice_request_.slice().offset()
            << " Failed";
    // Retry.
    controller_.Reset();
    ret = false;
  } else {
    ret = true;
  }
  VLOG(2) << "SyncSlice: " << status_->index() << " Done";
  file_transfer_->SyncSliceDone(shared_from_this(), ret, status_);
}

void FileTransferClient::SyncSliceDone(
    boost::shared_ptr<TransferTask> tasker, bool succeed, boost::shared_ptr<SliceStatus> status) {
  VLOG(2) << "SyncSlice Done";
  if (tasker->IsConnected()) {
    transfer_task_queue_.Push(tasker);
  }
  if (pool_.IsRunning()) {
    if (succeed) {
      status->set_status(SliceStatus::DONE);
    } else {
      status->set_status(SliceStatus::IDLE);
    }
    pool_.PushTask(boost::bind(&FileTransferClient::Schedule, this));
  } else {
    LOG(WARNING) << pool_.name() << " is stopped";
  }
}

TransferTask::TransferTask(
    FileTransferClient *file_transfer,
    FullDualChannel *channel, int id)
    : stub_(channel), id_(id), file_transfer_(file_transfer), channel_closed_(false) {
}

boost::shared_ptr<TransferTask> TransferTask::Create(
    FileTransferClient *file_transfer,
    FullDualChannel *channel, int id) {
  boost::shared_ptr<TransferTask> tasker(new TransferTask(
          file_transfer,
          channel, id));
  channel->close_signal()->connect(
      FullDualChannel::CloseSignal::slot_type(
          &TransferTask::ChannelClosed, tasker.get()).track(tasker));
  return tasker;
}
