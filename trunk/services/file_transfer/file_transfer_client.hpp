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
#ifndef FILE_TRANSFER_CLIENT_HPP_
#define FILE_TRANSFER_CLIENT_HPP_
#include "base/base.hpp"
#include "base/hash.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "thread/threadpool.hpp"
#include "server/protobuf_connection.hpp"
#include "services/file_transfer/checkbook.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
class TransferTask;
class SliceStatus;
class TimerMaster;
class FileTransferClient : public boost::enable_shared_from_this<FileTransferClient> {
 public:
  void PushChannel(Connection *channel);
  void Start();
  void Stop();
  void set_threadpool(boost::shared_ptr<ThreadPool> pool) {
    threadpool_ = pool;
  }
  void set_timer_master(boost::shared_ptr<TimerMaster> timer_master) {
    timer_master_ = timer_master;
  }
  void set_finish_listener(const boost::function0<void> h) {
    finish_handler_ = h;
  }
  bool finished() const {
    return status_ == FINISHED;
  }
  const string &host() const {
    return host_;
  }
  const string &port() const {
    return port_;
  }
  const string &src_filename() const {
    return src_filename_;
  }
  const string &dest_filename() const {
    return dest_filename_;
  }
  void set_timeout(int timeout) {
    timeout_ = timeout;
  }
  bool IsRunning() {
    return running_;
  }
  static FileTransferClient *Create(
      const string &host, const string &port,
      const string &src_filename,
      const string &dest_filename,
      int threadpool_size);
  // The percent * 1000, 1000 means transfer finished.
  int Percent();
  const string GetCheckBookDestFileName() const;
 private:
  static const int kDefaultTimeOutSec = 10;
  enum Status {
    SYNC_CHECKBOOK = 0,
    PREPARE_SLICE,
    SYNC_SLICE,
    FINISHED,
  };
  FileTransferClient(const string &host, const string &port,
                     const string &src_filename, const string &dest_filename,
                     int thread_pool_size);

  boost::shared_ptr<TimerMaster> timer_master() {
    return timer_master_;
  }
  void Schedule();
  void ScheduleTask();
  void SyncCheckBook();
  void PrepareSlice();
  void ScheduleSlice();
  void SyncSlice(boost::shared_ptr<SliceStatus> slice,
                 boost::shared_ptr<TransferTask> tasker);
  void SyncSliceDone(
      boost::shared_ptr<TransferTask> tasker,
      bool succeed, boost::shared_ptr<SliceStatus> status);
  void ChannelClosed(boost::shared_ptr<TransferTask> tasker,
                     boost::shared_ptr<SliceStatus> status);
  static const int kSyncCheckBookRetry = 3;
  typedef deque<boost::shared_ptr<TransferTask> > TransferTaskQueue;
  typedef list<boost::shared_ptr<SliceStatus> > SliceStatusLink;
  boost::shared_ptr<ThreadPool> threadpool_;
  boost::shared_ptr<TimerMaster> timer_master_;
  boost::function0<void> finish_handler_;
  PCQueue<boost::shared_ptr<TransferTask> > transfer_task_queue_;
  PCQueue<boost::shared_ptr<SliceStatus> > slice_status_queue_;
  boost::mutex transfer_task_set_mutex_;
  hash_set<boost::shared_ptr<TransferTask> > transfer_task_set_;
  scoped_ptr<CheckBook> checkbook_;
  boost::iostreams::mapped_file_source src_file_;
  boost::mutex sync_checkbook_mutex_;
  boost::mutex prepare_slice_mutex_;
  boost::mutex transfering_slice_mutex_;
  SliceStatusLink transfering_slice_;
  Status status_;
  int sync_checkbook_failed_;
  bool finished_;
  string host_, port_, src_filename_, dest_filename_;
  scoped_ptr<boost::asio::io_service> io_service_;
  scoped_ptr<boost::asio::io_service::work> work_;
  boost::shared_ptr<Notifier> notifier_;
  int timeout_;
  bool running_;
  boost::mutex running_mutex_;
  friend class TransferTask;
};
#endif  // FILE_TRANSFER_CLIENT_HPP_
