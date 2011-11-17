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
#ifndef FILE_DOWNLOAD_SERVICE_HPP_
#define FILE_DOWNLOAD_SERVICE_HPP_
#include "base/base.hpp"
#include "base/hash.hpp"
#include "thread/threadpool.hpp"
#include <boost/thread/mutex.hpp>
#include "server/protobuf_connection.hpp"
#include "services/file_transfer/file_transfer.pb.h"
#include "server/timer_master.hpp"
class FileTransferClient;
class DownloadTasker;
class FileDownloadServiceImpl : public FileTransfer::FileDownloadService , public Connection::AsyncCloseListener, public boost::enable_shared_from_this<FileDownloadServiceImpl> {
 public:
  FileDownloadServiceImpl(const string &doc_root,
                          int threadpool_size)
    : doc_root_(doc_root),
      threadpool_(new ThreadPool("FileDownloadServiceThreadPool", threadpool_size)),
      timer_master_(new TimerMaster) {
  }
  void RegisterDownload(google::protobuf::RpcController *controller,
                        const FileTransfer::RegisterRequest *request,
                        FileTransfer::RegisterResponse *response,
                        google::protobuf::Closure *done);
  void Stop();
  ~FileDownloadServiceImpl();
 private:
  void ConnectionClosed(Connection *channel);
  typedef hash_map<Connection*, hash_set<string> > ChannelTable;
  typedef hash_map<string, boost::shared_ptr<DownloadTasker> >
    DownloadTaskerTable;
  DownloadTaskerTable tasker_table_;
  boost::mutex table_mutex_;
  ChannelTable channel_table_;
  boost::shared_ptr<ThreadPool> threadpool_;
  boost::shared_ptr<TimerMaster> timer_master_;
  string doc_root_;
};

class FileDownloadNotifierInterface {
 public:
  virtual void DownloadComplete(
      const string &src_filename,
      const string &local_filename) = 0;
};

class FileDownloadNotifier : public FileDownloadNotifierInterface {
 public:
  FileDownloadNotifier(const string name = "FileDownloadNotifier")
     : notifier_(new Notifier(name)) {
  }
  void Wait() {
    notifier_->Wait();
  }
  void DownloadComplete(const string &src_filename, const string &local_filename) {
    notifier_->Notify();
  }
 protected:
  boost::shared_ptr<Notifier> notifier_;
};

class FileDownloadNotifyImpl : public FileTransfer::FileDownloadNotifyService {
 public:

  void DownloadComplete(google::protobuf::RpcController *controller,
                        const FileTransfer::DownloadCompleteRequest *request,
                        FileTransfer::DownloadCompleteResponse *response,
                        google::protobuf::Closure *done);
  void RegisterNotifier(
      const string &src_filename, const string &local_filename,
      boost::weak_ptr<FileDownloadNotifierInterface> notifier);
 private:
  boost::mutex mutex_;
  typedef hash_map<string, boost::weak_ptr<FileDownloadNotifierInterface> > NotifierTable;
  NotifierTable notifiers_;
};
#endif  // FILE_DOWNLOAD_SERVICE_HPP_
