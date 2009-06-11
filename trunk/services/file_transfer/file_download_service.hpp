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

class FileDownloadNotifyImpl : public FileTransfer::FileDownloadNotifyService {
  // The notify signal call back is f(src_filename, local_filename);
 typedef boost::signals2::signal<void()> NotifySignal;
 public:

  void DownloadComplete(google::protobuf::RpcController *controller,
                        const FileTransfer::DownloadCompleteRequest *request,
                        FileTransfer::DownloadCompleteResponse *response,
                        google::protobuf::Closure *done);
  NotifySignal *GetSignal(const string &src_filename, const string &local_filename);
 private:
  typedef hash_map<string, boost::shared_ptr<NotifySignal> > SignalTable;
  SignalTable signals_;
};
#endif  // FILE_DOWNLOAD_SERVICE_HPP_
