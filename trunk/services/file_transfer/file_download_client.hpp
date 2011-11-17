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



#ifndef FILE_DOWNLOAD_CLIENT_HPP_
#define FILE_DOWNLOAD_CLIENT_HPP_
#include "base/base.hpp"
#include "services/file_transfer/file_download_service.hpp"
class IOServicePool;
class TimerMaster;
class ClientConnection;
class FileTransferServiceImpl;
class FileDownloadNotifyImpl;
class Notifier;
class FileDownloadClient : public FileDownloadNotifierInterface, public boost::enable_shared_from_this<FileDownloadClient> {
 public:
  struct PercentItem {
    string src_filename;
    string local_filename;
    string checkbook_filename;
    int percent;
  };
  FileDownloadClient(
      const string &doc_root,
      const string &server,
      const string &port,
      int num_threads);
  bool Start();
  void Stop();
  void Reset();
  void Wait();
  bool DownloadFile(const string &src_filename, const string &local_filename);
  // The percent * 1000, 1000 means transfer finished for each pair of
  // src_filename, local_filename
  void Percent(vector<PercentItem> *items);
 private:
  int GetPercent(const string &checkbook_filename) const;
  void DownloadComplete(const string &src_filename,
                        const string &local_filename);
  boost::shared_ptr<IOServicePool> io_service_pool_;
  boost::shared_ptr<TimerMaster> timer_master_;
  boost::shared_ptr<FileTransferServiceImpl> transfer_service_;
  scoped_ptr<FileDownloadNotifyImpl> download_notify_;
  boost::shared_ptr<Notifier> notifier_;
  vector<boost::shared_ptr<ClientConnection> > connections_;
  boost::mutex running_mutex_;
  bool running_;
  string server_, port_, doc_root_;
  int num_threads_;
  typedef hash_map<string, PercentItem> PercentTable;
  boost::mutex percent_table_mutex_;
  PercentTable percent_table_;
  vector<boost::shared_ptr<RpcController> > controller_;
};
#endif  // FILE_DOWNLOAD_CLIENT_HPP_
