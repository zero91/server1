// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "services/file_transfer/file_download_service.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include "services/file_transfer/checkbook.hpp"
#include "server/server.hpp"
#include "server/client_connection.hpp"
#include "services/file_transfer/file_download_client.hpp"
#include "net/mac_address.hpp"

FileDownloadClient::FileDownloadClient(const string &doc_root,
                                       const string &server,
                                       const string &port,
                                       int num_threads)
: server_(server), port_(port), num_threads_(num_threads), doc_root_(doc_root), running_(false) {
  string name = "FileDownloadClient." + server + "." + port;
  io_service_pool_.reset(new IOServicePool(name + ".ThreadPool",
                                        1, num_threads)),
  timer_master_.reset(new TimerMaster),
  notifier_.reset(new Notifier(name + ".ThreadPool"));
  transfer_service_.reset(new FileTransferServiceImpl(doc_root_));
  download_notify_.reset(new FileDownloadNotifyImpl);
}

void FileDownloadClient::Stop() {
  boost::mutex::scoped_lock locker(running_mutex_);
  if (!running_) {
    LOG(WARNING) << "FileDownloadClient is stopped";
    return;
  }
  VLOG(2) << "FileDownloadClient::Stop, connection size: " << connections_.size();
  timer_master_->Stop();
  for (int i = 0; i < connections_.size(); ++i) {
    connections_[i]->Disconnect();
  }
  connections_.clear();
  io_service_pool_->Stop();
}

bool FileDownloadClient::Start() {
  boost::mutex::scoped_lock locker(running_mutex_);
  if (running_) {
    LOG(WARNING) << "FileDownloadClient is running.";
    return false;
  }
  CHECK(connections_.empty());
  timer_master_->Start();
  io_service_pool_->Start();
  for (int i = 0; i < connections_.size(); ++i) {
    connections_[i]->Disconnect();
  }
  connections_.clear();
  for (int i = 0; i < num_threads_; ++i) {
    const string name("FileDownloadClient." + boost::lexical_cast<string>(i));
    boost::shared_ptr<ClientConnection> r(
        new ClientConnection(name, server_, port_));
    r->set_io_service_pool(io_service_pool_);
    r->set_timer_master(timer_master_);
    CHECK(!r->IsConnected());
    if (!r->Connect()) {
      continue;
    }
    connections_.push_back(r);
  }
  running_ = true;
  return !connections_.empty();
}

bool FileDownloadClient::DownloadFile(
    const string &src_filename, const string &local_filename) {
  static const int kRegisterTimeout = 200;
  FileTransfer::RegisterRequest request;
  FileTransfer::RegisterResponse response;
  request.set_src_filename(src_filename);
  request.set_local_filename(local_filename);
  request.set_local_mac_address(GetMacAddress());
  download_notify_->RegisterNotifier(src_filename, local_filename, shared_from_this());
  string checkbook_filename;
  for (int i = 0; i < connections_.size(); ++i) {
    boost::shared_ptr<RpcController> controller(new RpcController);
    controller_.push_back(controller);
    const string name("FileDownloadTest2Client." + boost::lexical_cast<string>(i));
    FileTransfer::FileDownloadService::Stub stub(connections_[i].get());
    request.set_peer_name(connections_[i]->name());
    stub.RegisterDownload(
        controller.get(),
        &request, &response, NULL);
    if (!controller->Wait(kRegisterTimeout)) {
      LOG(WARNING) << "Connect timeout";
      continue;
    }
    VLOG(2) << "Register connection : " << i << " succeed";
    connections_[i]->RegisterService(transfer_service_.get());
    connections_[i]->RegisterService(download_notify_.get());
    checkbook_filename = response.checkbook_filename();
  }
  if (checkbook_filename.empty()) {
    return false;
  }
  PercentItem item;
  item.src_filename = src_filename;
  item.local_filename = local_filename;
  item.checkbook_filename = checkbook_filename;
  item.percent = 0;
  notifier_->Inc(1);
  boost::mutex::scoped_lock percent_table_locker(percent_table_mutex_);
  percent_table_[src_filename + local_filename] = item;
  return connections_.size();
}

void FileDownloadClient::DownloadComplete(
    const string &src_filename,
    const string &local_filename) {
  boost::mutex::scoped_lock locker(percent_table_mutex_);
  PercentTable::iterator it = percent_table_.find(src_filename + local_filename);
  CHECK(it != percent_table_.end());
  it->second.percent = 1000;
  notifier_->Dec(1);
}

void FileDownloadClient::Wait() {
  notifier_->Dec(1);
  notifier_->Wait();
}

int FileDownloadClient::GetPercent(
    const string &checkbook_filename) const {
  scoped_ptr<CheckBook> checkbook(CheckBook::Load(checkbook_filename));
  if (checkbook.get() == NULL) {
    return 0;
  }
  return checkbook->Percent();
}

void FileDownloadClient::Percent(
    vector<PercentItem> *items) {
  boost::mutex::scoped_lock locker(percent_table_mutex_);
  for (PercentTable::iterator it = percent_table_.begin();
       it != percent_table_.end(); ++it) {
    if (it->second.percent != 1000) {
      it->second.percent = GetPercent(it->second.checkbook_filename);
    }
    items->push_back(it->second);
  }
}
