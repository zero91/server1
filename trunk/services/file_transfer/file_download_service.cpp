#include "server/full_dual_channel_proxy.hpp"
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
  void AddChannel(FullDualChannel *channel) {
    boost::shared_ptr<FullDualChannelProxy> proxy(FullDualChannelProxy::Create(channel));
    boost::mutex::scoped_lock locker(mutex_);
    channels_.insert(proxy);
  }
  void RemoveChannel(FullDualChannel *channel) {
    boost::shared_ptr<FullDualChannelProxy> proxy(FullDualChannelProxy::Create(channel));
    boost::mutex::scoped_lock locker(mutex_);
    channels_.erase(proxy);
  }
  bool channel_size() const {
    return channels_.size();
  }
  FileTransferClient *client() const {
    return client_.get();
  }
  ~DownloadTasker() {
    VLOG(2) << "~DownloadTasker";
  }
 private:
  void DownloadFinished() {
    VLOG(2) << "DownloadFinished";
    boost::mutex::scoped_lock locker(mutex_);
    FileTransfer::DownloadCompleteRequest request;
    request.set_src_filename(client_->src_filename());
    request.set_local_filename(client_->dest_filename());
    for (hash_set<boost::shared_ptr<FullDualChannelProxy> >::iterator it = channels_.begin();
         it != channels_.end(); ++it) {
      boost::shared_ptr<RpcController> controller(new RpcController);
      boost::shared_ptr<FileTransfer::DownloadCompleteResponse> response(
          new FileTransfer::DownloadCompleteResponse);
      boost::shared_ptr<FullDualChannelProxy> proxy = *it;
      FileTransfer::FileDownloadNotifyService::Stub stub(proxy.get());
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
  hash_set<boost::shared_ptr<FullDualChannelProxy> > channels_;
  boost::mutex mutex_;
  scoped_ptr<FileTransferClient> client_;
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

void FileDownloadServiceImpl::CloseChannel(
    FullDualChannel *channel) {
  bool is_idle = false;
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
          jt->second->client()->Stop();
          tasker_table_.erase(jt);
          VLOG(0) << "Client arrive zero channel, remove from tasker table";
        } else {
          VLOG(2) << "Client channel size: " << jt->second->channel_size();
        }
      }
    }
    channel_table_.erase(it);
    if (channel_table_.empty()) {
      is_idle = true;
    }
  }
  if (is_idle) {
    VLOG(1) << "Is Idle, remove channel";
    threadpool_.Stop();
  }
}

FileDownloadServiceImpl::~FileDownloadServiceImpl() {
  if (threadpool_.IsRunning()) {
    VLOG(1) << "Stop thread pool in FileDownloadServiceImpl";
    threadpool_.Stop();
  }
}

void FileDownloadServiceImpl::RegisterDownload(
    google::protobuf::RpcController *controller,
    const FileTransfer::RegisterRequest *request,
    FileTransfer::RegisterResponse *response,
    google::protobuf::Closure *done) {
  ScopedClosure run(done);
  FullDualChannel *channel = dynamic_cast<FullDualChannel*>(controller);
  VLOG(2) << "RegisterDownload, channel: " << channel->Name() << " peer: " << request->peer_name();;
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
      FileTransferClient *client =
          FileTransferClient::Create(host,"", src_filename, dest_filename, 0);
      tasker = DownloadTasker::Create(client);
      if (!threadpool_.IsRunning()) {
        threadpool_.Start();
      }
      tasker->client()->set_threadpool(&threadpool_);
      init = true;
      tasker_table_.insert(make_pair(unique_identify, tasker));
    } else {
      boost::shared_ptr<DownloadTasker> local_tasker = it->second;
      FileTransferClient *local_client = local_tasker->client();
      if (local_client->host() == host &&
          local_client->port().empty() &&
          local_client->src_filename() == src_filename &&
          local_client->dest_filename() == dest_filename) {
        tasker = local_tasker;
      } else {
        LOG(WARNING) << "Name conflict";
        response->set_succeed(false);
        return;
      }
    }
    channel_table_[channel].insert(unique_identify);
    channel->close_signal()->connect(boost::bind(
        &FileDownloadServiceImpl::CloseChannel, this, channel));
  }
  if (init) {
    tasker->client()->Start();
  }
  // This channel is used to notify.
  tasker->AddChannel(channel);
  tasker->client()->PushChannel(channel);
  response->set_succeed(true);
}

void FileDownloadNotifyImpl::DownloadComplete(google::protobuf::RpcController *controller,
                                               const FileTransfer::DownloadCompleteRequest *request,
                                               FileTransfer::DownloadCompleteResponse *response,
                                               google::protobuf::Closure *done) {
  ScopedClosure run(done);
  const string src_filename = request->src_filename();
  const string local_filename = request->local_filename();
  response->set_succeed(true);
  string key(src_filename + local_filename);
  SignalTable::iterator it = signals_.find(key);
  if (it != signals_.end()) {
    (*it->second)();
  }
}

FileDownloadNotifyImpl::NotifySignal *FileDownloadNotifyImpl::GetSignal(
    const string &src_filename,
    const string &local_filename) {
  string key = src_filename + local_filename;
  SignalTable::iterator it = signals_.find(key);
  if (it == signals_.end()) {
    boost::shared_ptr<NotifySignal> sig(new NotifySignal);
    signals_.insert(make_pair(key, sig));
    it = signals_.find(key);
  }
  return it->second.get();
}
