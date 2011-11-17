/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */
#include "services/file_transfer/file_transfer_client.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "server/timer.hpp"
#include "server/timer_master.hpp"
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

class TransferTask : virtual public Connection::AsyncCloseListener, virtual public Timer, public boost::enable_shared_from_this<TransferTask> {
 public:
  static boost::shared_ptr<TransferTask> Create(
      boost::weak_ptr<FileTransferClient> file_transfer,
      Connection *channel, int id, int timeout);

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
    return connection_->IsConnected();
  }
  ~TransferTask() {
    VLOG(2) << "~TransferTask" << id_;
  }
  void ConnectionClosed(Connection *connection) {
    if (connection != connection_.get()) {
      LOG(WARNING) << "Expired connection " << connection->name();
      return;
    }
    boost::shared_ptr<FileTransferClient> client = file_transfer_.lock();
    VLOG(2) << "ChannelClosed channel: " << connection_->name() << " tasker:" << id_ << " slice: " << (status_.get() ? status_->index() : -1);
    if (file_transfer_.expired()) {
      LOG(WARNING) << "FileTransfer had expired";
      return;
    }
    if (!client->IsRunning()) {
      LOG(WARNING) << "FileTransfer had stopped";
      return;
    }
    client->ChannelClosed(shared_from_this(), status_);
  }
 private:
  TransferTask(
      boost::weak_ptr<FileTransferClient> file_transfer,
      boost::shared_ptr<Connection> connection, int id, int timeout);
  virtual int timeout() const {
    return timeout_;
  }
  virtual bool period() const {
    return period_;
  }
  virtual void Expired();
  void SyncSliceDone();
  static const int kRetry = 2;
  boost::shared_ptr<Connection> connection_;
  FileTransfer::FileTransferService::Stub stub_;
  FileTransfer::SliceRequest slice_request_;
  FileTransfer::SliceResponse slice_response_;
  FileTransfer::CheckBookResponse checkbook_response_;
  RpcController controller_;
  boost::weak_ptr<FileTransferClient> file_transfer_;
  boost::shared_ptr<SliceStatus> status_;
  int id_;
  int timeout_;
  bool period_;
};

void FileTransferClient::ChannelClosed(
    boost::shared_ptr<TransferTask> tasker,
    boost::shared_ptr<SliceStatus> status) {
  VLOG(2) << "FileTransferClient::ChannelClosed, tasker: " << tasker->id();
  {
    boost::mutex::scoped_lock locker(transfer_task_set_mutex_);
    transfer_task_set_.erase(tasker);
  }
  {
    boost::mutex::scoped_lock locker(transfering_slice_mutex_);
    if (status.get() != NULL && status->status() != SliceStatus::DONE) {
      VLOG(1) << "Reset slice: " << status->index() << " to idle";
      status->set_status(SliceStatus::IDLE);
    }
  }
  if (IsRunning()) {
    ScheduleTask();
  }
}

void FileTransferClient::PushChannel(Connection *channel) {
  boost::mutex::scoped_lock running_locker(running_mutex_);
  static int id = 0;
  if (!channel->IsConnected()) {
    LOG(WARNING) << "Channel is not connected";
    return;
  }
  boost::shared_ptr<TransferTask> tasker(TransferTask::Create(
          shared_from_this(),
          channel,
          id++, timeout_));
  {
    boost::mutex::scoped_lock locker(transfer_task_set_mutex_);
    transfer_task_set_.insert(tasker);
  }
  transfer_task_queue_.Push(tasker);
  if (IsRunning()) {
    ScheduleTask();
  }
}

FileTransferClient::FileTransferClient(const string &host, const string &port,
                                       const string &src_filename, const string &dest_filename,
                                       int thread_pool_size) :
  host_(host), port_(port), src_filename_(src_filename),
  dest_filename_(dest_filename),
  threadpool_(new ThreadPool(
      "FileTransferClientThreadPool", thread_pool_size)),
  sync_checkbook_failed_(0), finished_(false), status_(SYNC_CHECKBOOK),
  timeout_(kDefaultTimeOutSec),
  timer_master_(new TimerMaster), running_(false) {
}


void FileTransferClient::Start() {
  {
    boost::mutex::scoped_lock running_locker(running_mutex_);
    if (running_) {
      LOG(WARNING) << "FileTransferClient is already running";
      return;
    }
    notifier_.reset(new Notifier("FileTransferClientJob"));
    threadpool_->Start();
    timer_master_->Start();
    io_service_.reset(new boost::asio::io_service);
    work_.reset(new boost::asio::io_service::work(*io_service_));
    threadpool_->PushTask(boost::bind(
        &boost::asio::io_service::run, io_service_.get()));
    running_ = true;
  }
  boost::mutex::scoped_lock locker(transfer_task_set_mutex_);
  for (int i = 0; i < transfer_task_set_.size(); ++i) {
    ScheduleTask();
  }
}

void FileTransferClient::Stop() {
  boost::mutex::scoped_lock locker(running_mutex_);
  if (!running_) {
    LOG(WARNING) << "FileTransferClient is already stop";
    return;
  }
  running_ = false;
  boost::shared_ptr<TransferTask> tasker;
  int count = notifier_->count();
  for (int i = 0; i < count; ++i) {
    transfer_task_queue_.Push(tasker);
  }
  work_.reset();
  notifier_->Dec(1);
  notifier_->Wait();
  if (!finished()) {
    VLOG(1) << "SaveCheckBook to: " << checkbook_->GetCheckBookSrcFileName();
    checkbook_->Save(checkbook_->GetCheckBookSrcFileName());
    VLOG(1) << "SaveCheckBook to: " << checkbook_->GetCheckBookSrcFileName() << " Succeed";
  }
  VLOG(2) << "FileTransferClient stopped, Percent: " << Percent();
}

FileTransferClient *FileTransferClient::Create(
    const string &host, const string &port,
    const string &src_filename,
    const string &dest_filename,
    int threadpool_size) {
  FileTransferClient *file_transfer = new FileTransferClient(
      host, port, src_filename, dest_filename, threadpool_size);
  file_transfer->checkbook_.reset(
      CheckBook::Create(host, port, src_filename, dest_filename));
  if (!file_transfer->checkbook_.get()) {
    delete file_transfer;
    return NULL;
  }
  return file_transfer;
}

const string FileTransferClient::GetCheckBookDestFileName() const {
  return checkbook_->GetCheckBookDestFileName();
}

int FileTransferClient::Percent() {
  if (finished()) {
    return 1000;
  }
  return checkbook_->Percent();
}

void FileTransferClient::ScheduleTask() {
  CHECK(IsRunning());
  notifier_->Inc(1);
  threadpool_->PushTask(boost::bind(&FileTransferClient::Schedule, this));
}

void FileTransferClient::Schedule() {
  VLOG(2) << "Schedule, status: " << status_;
  switch (status_) {
    case SYNC_CHECKBOOK:
      SyncCheckBook();
      break;
    case PREPARE_SLICE:
      PrepareSlice();
      break;
    case SYNC_SLICE:
      ScheduleSlice();
      break;
    case FINISHED:
      VLOG(1) << "Finished";
      break;
  }
  notifier_->Dec(1);
  return;
}

void FileTransferClient::SyncCheckBook() {
  boost::mutex::scoped_lock locker(sync_checkbook_mutex_);
  if (status_ != SYNC_CHECKBOOK) {
    LOG(WARNING) << "SyncCheckBook but status is: " << status_;
    ScheduleTask();
    return;
  }
  if (sync_checkbook_failed_ >= kSyncCheckBookRetry) {
    LOG(WARNING) << "SyncCheckbook failed: " << sync_checkbook_failed_;
    ScheduleTask();
    return;
  }
  if (checkbook_->meta().synced_with_dest()) {
    LOG(WARNING) << "Already synced with dest";
    status_ = PREPARE_SLICE;
    ScheduleTask();
    return;
  }
  boost::shared_ptr<TransferTask> tasker = transfer_task_queue_.Pop();
  if (tasker.get() == NULL) {
    LOG(WARNING) << "Get null tasker, return";
    return;
  }
  if (!tasker->SyncCheckBook(checkbook_.get())) {
    LOG(WARNING) << "Transfer checkbook failed, tasker: " << tasker->IsConnected();
    ++sync_checkbook_failed_;
    if (tasker->IsConnected()) {
      transfer_task_queue_.Push(tasker);
    }
    if (IsRunning()) {
      ScheduleTask();
    }
  } else {
    status_ = PREPARE_SLICE;
    checkbook_->mutable_meta()->set_synced_with_dest(true);
    checkbook_->Save(checkbook_->GetCheckBookSrcFileName());
    if (tasker->IsConnected()) {
      transfer_task_queue_.Push(tasker);
    }
    ScheduleTask();
  }
}

bool TransferTask::SyncCheckBook(
    const FileTransfer::CheckBook *checkbook) {
  VLOG(1) << "SyncCheckBook";
  checkbook_response_.Clear();
  controller_.Reset();
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

void FileTransferClient::PrepareSlice() {
  boost::mutex::scoped_lock locker(prepare_slice_mutex_);
  if (status_ != PREPARE_SLICE) {
    LOG(WARNING) << "PrepareSlice but status is: " << status_;
    ScheduleTask();
    return;
  }
  VLOG(1) << "PrepareSlice";
  // Open the source file.
  const FileTransfer::MetaData &meta = checkbook_->meta();
  src_file_.open(meta.src_filename());
  if (!src_file_.is_open()) {
    LOG(WARNING) << "Fail to open source file: "
      << meta.src_filename();
    ScheduleTask();
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
  ScheduleTask();
}

void FileTransferClient::ScheduleSlice() {
  boost::shared_ptr<TransferTask> tasker = transfer_task_queue_.Pop();
  if (tasker.get() == NULL) {
    LOG(WARNING) << "Get null tasker, return";
    return;
  }
  VLOG(2) << "Get tasker " << tasker->id() << " tasker queue size: " << transfer_task_queue_.size();
  boost::shared_ptr<SliceStatus> slice;
  bool in_transfering = false;
  bool call_finish_handler = false;
  {
    boost::mutex::scoped_lock locker(transfering_slice_mutex_);
    VLOG(2) << "slice list size: " << transfering_slice_.size();
    if (status_ != SYNC_SLICE) {
      LOG(WARNING) << "ScheduleSlice but status is: " << status_;
      ScheduleTask();
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
    if (slice.get() == NULL && !in_transfering && !finished()) {
      VLOG(1) << "Transfer success!";
      status_ = FINISHED;
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
    VLOG(2) << "Get null slice, push back the tasker";
    boost::this_thread::yield();
    if (tasker->IsConnected()) {
      transfer_task_queue_.Push(tasker);
    }
    return;
  }
  VLOG(2) << "Transfer, tasker: " << tasker->id() << " slice: " << slice->index();
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

void TransferTask::Expired() {
  VLOG(2) << connection_->name() << " : " << "TransferTask Timeouted";
  if (status_.get() == NULL) {
    VLOG(2) << "Transfer : " << id() << " timeout but have null status";
    return;
  }
  connection_->Disconnect();
  VLOG(2) << "slice " << status_->index() << " timeouted, status: "
    << status_->status();
  boost::shared_ptr<FileTransferClient> client = file_transfer_.lock();
  if (file_transfer_.expired()) {
    LOG(WARNING) << "TransferTask timeout but file_transfer is expired";
    period_ = false;
    return;
  }
  if (!client->IsRunning()) {
    LOG(WARNING) << "TransferTask timeout but file_transfer is stopped";
    period_ = false;
    return;
  }
  client->ChannelClosed(shared_from_this(), status_);
}

void TransferTask::SyncSlice() {
  VLOG(1) << "SyncSlice: Channel: " << connection_->name() << " tasker: " << id() << " slice: " << status_->index()
      << "timeout: " << timeout_;
  controller_.Reset();
  slice_response_.Clear();
  stub_.ReceiveSlice(&controller_,
                     &slice_request_,
                     &slice_response_,
                     NewClosure(boost::bind(
                         &TransferTask::SyncSliceDone,
                         shared_from_this())));
}

void TransferTask::SyncSliceDone() {
  VLOG(1) << "SyncSliceDone: Channel: " << connection_->name() << " tasker: " << id() << " slice: " << status_->index();
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
  VLOG(2) << "SyncSliceDone: " << id() << " " << status_->index() << " " << ret;
  if (status_->status() == SliceStatus::TRANSFERING) {
    VLOG(2) << "Reschedule slice: " << status_->index();
    boost::shared_ptr<FileTransferClient> lock_file_transfer = file_transfer_.lock();
    if (file_transfer_.expired()) {
      VLOG(2) << "FileTransferClient had expired";
      return;
    }
    if (!lock_file_transfer->IsRunning()) {
      VLOG(2) << "FileTransferClient had stopped";
      return;
    }
    lock_file_transfer->SyncSliceDone(shared_from_this(), ret, status_);
  }
}

void FileTransferClient::SyncSliceDone(
    boost::shared_ptr<TransferTask> tasker, bool succeed, boost::shared_ptr<SliceStatus> status) {
  VLOG(2) << "SyncSlice: " << status->index() << " result: " << succeed;
  if (tasker->IsConnected()) {
    transfer_task_queue_.Push(tasker);
  }
  {
    boost::mutex::scoped_lock locker(transfering_slice_mutex_);
    if (succeed) {
      status->set_status(SliceStatus::DONE);
    } else {
      status->set_status(SliceStatus::IDLE);
    }
  }
  VLOG(2) << "SyncSlice: " << status->index() << " result: " << succeed << " Push Task";
  ScheduleTask();
}

TransferTask::TransferTask(
    boost::weak_ptr<FileTransferClient> file_transfer,
    boost::shared_ptr<Connection> connection, int id, int timeout)
    : connection_(connection), stub_(connection_.get()), id_(id), file_transfer_(file_transfer),
      timeout_(timeout), period_(true) {
}

boost::shared_ptr<TransferTask> TransferTask::Create(
    boost::weak_ptr<FileTransferClient> file_transfer,
    Connection *connection, int id, int timeout) {
  boost::shared_ptr<TransferTask> tasker(new TransferTask(
      file_transfer,
      connection->shared_from_this(), id, timeout));
  boost::shared_ptr<FileTransferClient> lock_file_transfer = file_transfer.lock();
  if (file_transfer.expired()) {
    LOG(WARNING) << "FileTransferClient had expired";
    tasker.reset();
    return tasker;
  }
  lock_file_transfer->timer_master()->Register(tasker);
  if (!connection->RegisterAsyncCloseListener(
      tasker)) {
    LOG(WARNING) << "Fail to register close signal";
    tasker.reset();
  }
  return tasker;
}
