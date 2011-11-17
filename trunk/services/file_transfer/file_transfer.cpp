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
#include "services/file_transfer/file_transfer.hpp"
#include <boost/iostreams/device/mapped_file.hpp>


FileTransfer::TransferTask::TransferTask(
    const string &host, const string &port)
    : connection_(host, port),
      stub_(&connection_), id_(id) {
}

void FileTransfer::TransferTask::Transfer() {
  status_->set_status(SliceStatus::TRANSFERING);
  stub_.ReceiveSlice(&controller_,
                     &request_,
                     &response_,
                     NewClosure(boost::bind(
                         &FileTransfer::TransferTask::TransferDone,
                         this)));
}

void FileTransfer::TransferTask::SyncCheckBook(
    const FileTransfer::CheckBook *checkbook) {
  stub_.ReceiveCheckBook(&controller_,
                         checkbook_,
                         &checkbook_response_,
                         NewClosure(boost::bind(
                             &FileTransfer::TransferTask::SyncCheckBookDone,
                             this)));
}

void FileTransfer::TransferTask::SyncCheckBookDone() {
  if (controller_.IsFailed()) {
    VLOG(2) << "transfer id: " << id << " sync checkbook failed";
    controller_.Reset();
    file_transfer_->SyncCheckBookDone(false);
  } else {
    file_transfer_->SyncCheckBookDone(true);
  }
}

void FileTransfer::TransferTask::TransferDone() {
  if (controller_->IsFailed()) {
    VLOG(2) << "transfer id: " << id << " slice: " << request_->slice().offset()
            << " Failed";
    // Retry.
    controller_.Reset();
    status_->set_status(SliceStatus::IDLE);
  } else {
    status_->set_status(SliceStatus::DONE);
  }
  file_transfer_->TransferSliceDone(this);
}

FileTransfer::FileTransfer() {
}

void FileTransfer::Start(int threads) {
  threads_ = threads;
  if (checkbook_->meta().synced_with_dest()) {
    status_ = SYNC_SLICE;
  } else {
    status_ = SYNC_CHECKBOOK;
  }
  Schedule();
}

void FileTransfer::Schedule() {
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

void FileTransfer::SyncCheckBook() {
  boost::shared_ptr<TransferTask> tasker(
      new TransferTask(meta.host(), meta.port()));
  transfer_task_list_.push_back(tasker);
  tasker->SyncCheckBook(*check_book_.get());
}

void FileTransfer::SyncCheckBookDone(
    boost::shared_ptr<TransferTask> tasker,
    bool succeed) {
  if (succeed) {
    status_ = SYNC_SLICE;
    checkbook_->mutable_meta()->set_synced_with_dest(true);
    checkbook_->Save(checkbook_->GetCheckBookSrcFileName());
    transfer_task_queue_.Push(takser);
  } else {
    ++sync_checkbook_failed_;
  }
  Schedule();
  return;
}

void FileTransfer::SyncSlice() {
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
    shared_ptr<SliceStatus> slice_status(
        new SliceStatus(checkbook_->slice(i).index()));
    transfering_slice_.push_back(slice_status);
  }

  for (int i = 0; i < threads; ++i) {
    boost::shared_ptr<TransferTask> tasker(
        new TransferTask(meta.host(), meta.port()));
    transfer_task_queue_.Push(transfer_slice);
    transfer_task_list_.push_back(tasker);
  }
  pool_.PushTask(boost::bind(
      &FileTransfer::ScheduleSlice, this));
}

void FileTransfer::TransferSlice(
    boost::shared_ptr<SliceStatus> slice,
    TransferTask *tasker) {
  SliceRequest *request = tasker->mutable_request();
  request->Clear();
  const int index = slice->index();
  const FileTransfer::Slice &slice_meta = checkbook_->slice(index);
  const int length = slice_meta.length();
  const int offset = slice_meta.offset();
  request->mutable_slice()->CopyFrom(slice_meta);
  request->mutable_content()->assign(
      src_file_.data() + offset, length);
  trasker->Transfer();
}

void FileTransfer::TransferSliceDone(
    boost::shared_ptr<TransferTask> tasker) {
  transfer_task_queue_.Push(tasker);
}

void FileTransfer::ScheduleSlice() {
  for (;;) {
    boost::shared_ptr<SliceStatus> slice;
    for (SliceStatusLink::iterator it = transfering_slice_.begin();
         it != transfering_slice_.end();) {
      SliceStatusLink::iterator next = it + 1;
      if (it->status() == SliceStatus::DONE) {
        VLOG(2) << "slice " << it->index() << " Done";
        transfering_slice_.erase(it);
        it = next;
        continue;
      } else if (it->status() == SliceStatus::TRANSFERING) {
        VLOG(2) << "slice " << it->index() << " Transfering";
        it = next;
        continue;
      } else {
        slice = *it;
        slice->set_status(SliceStatus::TRANSFERING);
        break;
      }
    }
    if (slice.get() == NULL) {
      VLOG(2) << "Transfer success!";
      if (!finish_handler_.empty()) {
        finish_handler_();
      }
      return;
    }
    VLOG(2) << "Get slice: " << slice->index();
    boost::shared_ptr<TranfserTask> tasker = transfer_task_queue_.Pop();
    VLOG(2) << "Get task: " << tasker->id();
    pool_.PushTask(boost::bind(
        &FileTransfer::TransferSlice, this,
        slice, tasker));
  }
}

FileTransfer *FileTransfer::Create(
    const string &host, const string &port,
    const string &src_filename,
    const string &dest_filename) {
  FileTransfer *file_tranfer = new FileTransfer;
  file_transfer.checkbook_.reset(CheckBook::Create(
      host, port, src_filename, dest_filename));
  if (file_transfer.checkbook_.get() == NULL) {
    return NULL;
  }
  return file_transfer_;
}
