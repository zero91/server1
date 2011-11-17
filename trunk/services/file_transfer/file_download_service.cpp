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
#include "server/protobuf_connection.hpp"
#include "services/file_transfer/file_download_service.hpp"
#include "services/file_transfer/file_transfer_client.hpp"
#include "crypto/evp.hpp"
class DownloadTasker {
 public:
  static boost::shared_ptr<DownloadTasker> Create(FileTransferClient *client) {
    boost::shared_ptr<DownloadTasker> tasker(new DownloadTasker(client));
    tasker->client()->set_finish_listener(boost::bind(&DownloadTasker::DownloadFinished, tasker.get()));
    return tasker;
  }
  void AddChannel(Connection *channel) {
    boost::mutex::scoped_lock locker(mutex_);
    channels_.insert(channel->shared_from_this());
  }
  void RemoveChannel(Connection *channel) {
    boost::mutex::scoped_lock locker(mutex_);
    channels_.erase(channel->shared_from_this());
  }
  int channel_size() const {
    return channels_.size();
  }
  boost::shared_ptr<FileTransferClient> client() const {
    return client_;
  }
  void reset_client() {
    client_.reset();
  }
  ~DownloadTasker() {
    VLOG(2) << "~DownloadTasker";
    channels_.clear();
    if (client_.get()) {
      client_->Stop();
    }
  }
 private:
  typedef hash_set<boost::shared_ptr<Connection> > ChannelTable;
  void DownloadFinished() {
    VLOG(2) << "DownloadFinished";
    boost::mutex::scoped_lock locker(mutex_);
    FileTransfer::DownloadCompleteRequest request;
    request.set_src_filename(client_->src_filename());
    request.set_local_filename(client_->dest_filename());
    for (ChannelTable::iterator it = channels_.begin();
         it != channels_.end(); ++it) {
      boost::shared_ptr<RpcController> controller(new RpcController);
      boost::shared_ptr<FileTransfer::DownloadCompleteResponse> response(
          new FileTransfer::DownloadCompleteResponse);
      FileTransfer::FileDownloadNotifyService::Stub stub(it->get());
      stub.DownloadComplete(controller.get(), &request, response.get(),
                            NewClosure(boost::bind(&DownloadTasker::DownloadCompleteDone, this, controller, response)));
    }
  }

  void DownloadCompleteDone(
      boost::shared_ptr<RpcController> controller,
      boost::shared_ptr<FileTransfer::DownloadCompleteResponse> response) {
    VLOG(2) << "DownloadComplete done! " << response->succeed();
  }

  DownloadTasker(FileTransferClient *client) : client_(client) {
  }
  ChannelTable channels_;
  boost::mutex mutex_;
  boost::shared_ptr<FileTransferClient> client_;
};

static string GetRegisterUniqueIdentify(
    const string &host, const string &src_filename, const string &dest_filename) {
  scoped_ptr<EVP> evp(EVP::CreateMD5());
  evp->Update(host);
  evp->Update(src_filename);
  evp->Update(dest_filename);
  evp->Finish();
  return evp->digest<string>();
}

void FileDownloadServiceImpl::ConnectionClosed(
    Connection *channel) {
  bool is_idle = false;
  boost::shared_ptr<FileTransferClient> client;
  {
    boost::mutex::scoped_lock locker(table_mutex_);
    ChannelTable::iterator it = channel_table_.find(channel);
    if (it == channel_table_.end()) {
      VLOG(2) << "Can't find channel: " << channel;
      return;
    }
    hash_set<string> *files = &it->second;
    for (hash_set<string>::iterator it = files->begin();
         it != files->end(); ++it) {
      const string &unique_identify = *it;
      DownloadTaskerTable::iterator jt = tasker_table_.find(unique_identify);
      if (jt != tasker_table_.end()) {
        jt->second->RemoveChannel(channel);
        if (jt->second->channel_size() == 0) {
          client = jt->second->client();
          jt->second->reset_client();
          threadpool_->PushTask(boost::bind(&FileTransferClient::Stop, client));
          tasker_table_.erase(jt);
          VLOG(0) << "Client arrive zero channel, remove from tasker table";
        } else {
          VLOG(2) << "Client channel size: " << jt->second->channel_size();
        }
      }
    }
    channel_table_.erase(it);
  }
}

FileDownloadServiceImpl::~FileDownloadServiceImpl() {
  CHECK(!threadpool_->IsRunning());
  CHECK(!timer_master_->IsRunning());
}

void FileDownloadServiceImpl::Stop() {
  vector<boost::shared_ptr<FileTransferClient> > clients;
  {
    boost::mutex::scoped_lock locker(table_mutex_);
    for (DownloadTaskerTable::iterator it = tasker_table_.begin();
         it != tasker_table_.end(); ++it) {
      clients.push_back(it->second->client());
    }
  }

  for (int i = 0; i < clients.size(); ++i) {
    clients[i]->Stop();
  }
  if (threadpool_->IsRunning()) {
    threadpool_->Stop();
  }
  if (timer_master_->IsRunning()) {
    timer_master_->Stop();
  }
  channel_table_.clear();
  tasker_table_.clear();
}

void FileDownloadServiceImpl::RegisterDownload(
    google::protobuf::RpcController *controller,
    const FileTransfer::RegisterRequest *request,
    FileTransfer::RegisterResponse *response,
    google::protobuf::Closure *done) {
  ScopedClosure run(done);
  Connection *channel = dynamic_cast<Connection*>(controller);
  VLOG(2) << "RegisterDownload, channel: " << channel->name() << " peer: " << request->peer_name();;
  if (channel == NULL) {
    response->set_succeed(false);
    LOG(WARNING) << "Can't convert controller to full dual channel.";
    return;
  }
  const string host = request->local_mac_address();
  const string src_filename = request->src_filename();
  const string dest_filename = request->local_filename();
  string unique_identify = GetRegisterUniqueIdentify(host, src_filename, dest_filename);
  bool init = false;
  boost::shared_ptr<DownloadTasker> tasker;
  {
    boost::mutex::scoped_lock locker(table_mutex_);
    DownloadTaskerTable::iterator it = tasker_table_.find(unique_identify);
    if (it == tasker_table_.end()) {
      VLOG(2) << "Create transfer client for: " << host << " " << src_filename << " " << dest_filename << " " << unique_identify;
      FileTransferClient *client =
          FileTransferClient::Create(host,"", src_filename, dest_filename, 0);
      tasker = DownloadTasker::Create(client);
      if (!threadpool_->IsRunning()) {
        threadpool_->Start();
      }
      if (!timer_master_->IsRunning()) {
        timer_master_->Start();
      }
      tasker->client()->set_threadpool(threadpool_);
      tasker->client()->set_timer_master(timer_master_);
      init = true;
      tasker_table_.insert(make_pair(unique_identify, tasker));
    } else {
      boost::shared_ptr<DownloadTasker> local_tasker = it->second;
      boost::shared_ptr<FileTransferClient> local_client = local_tasker->client();
      if (local_client->host() == host &&
          local_client->port().empty() &&
          local_client->src_filename() == src_filename &&
          local_client->dest_filename() == dest_filename) {
        VLOG(2) << "Find transfer client for: " << host << " " << src_filename << " " << dest_filename << " " << unique_identify;
        tasker = local_tasker;
      } else {
        LOG(WARNING) << "Name conflict";
        response->set_succeed(false);
        return;
      }
    }
    channel_table_[channel].insert(unique_identify);
    channel->RegisterAsyncCloseListener(
        shared_from_this());
  }
  if (init) {
    tasker->client()->Start();
  }
  // This channel is used to notify.
  tasker->AddChannel(channel);
  VLOG(2) << "tasker channel size: " << tasker->channel_size();
  tasker->client()->PushChannel(channel);
  response->set_succeed(true);
  response->set_checkbook_filename(tasker->client()->GetCheckBookDestFileName());
}

void FileDownloadNotifyImpl::DownloadComplete(
    google::protobuf::RpcController *controller,
    const FileTransfer::DownloadCompleteRequest *request,
    FileTransfer::DownloadCompleteResponse *response,
    google::protobuf::Closure *done) {
  boost::mutex::scoped_lock locker(mutex_);
  ScopedClosure run(done);
  const string src_filename = request->src_filename();
  const string local_filename = request->local_filename();
  response->set_succeed(true);
  string key(src_filename + local_filename);
  NotifierTable::const_iterator it = notifiers_.find(key);
  if (it != notifiers_.end()) {
    VLOG(2) << "Call notify for key: " << key;
    boost::weak_ptr<FileDownloadNotifierInterface> weak_notifier= it->second;
    boost::shared_ptr<FileDownloadNotifierInterface> notifier = weak_notifier.lock();
    if (weak_notifier.expired()) {
      LOG(WARNING) << "Notify for key: " << key << " expired";
      return;
    }
    notifier->DownloadComplete(src_filename, local_filename);
  } else {
    VLOG(2) << "Can't find notify for key: " << key;
  }
}

void FileDownloadNotifyImpl::RegisterNotifier(
    const string &src_filename,
    const string &local_filename,
    boost::weak_ptr<FileDownloadNotifierInterface> notifier) {
  boost::mutex::scoped_lock locker(mutex_);
  string key = src_filename + local_filename;
  NotifierTable::iterator it = notifiers_.find(key);
  CHECK(it == notifiers_.end());
  notifiers_.insert(make_pair(key, notifier));
  VLOG(2) << "Set notify for key: " << key;
}
