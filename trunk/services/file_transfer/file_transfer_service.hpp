#ifndef FILE_TRANSFER_SERVICE_HPP_
#define FILE_TRANSFER_SERVICE_HPP_
#include "base/base.hpp"
#include "base/hash.hpp"
#include <boost/thread/mutex.hpp>
#include "server/protobuf_connection.hpp"
#include "services/file_transfer/file_transfer.pb.h"
class TransferInfo;
class Connection;
class FileTransferServiceImpl : public FileTransfer::FileTransferService {
 public:
  FileTransferServiceImpl(const string &doc_root) : doc_root_(doc_root) {
  }
  void ReceiveCheckBook(google::protobuf::RpcController *controller,
                        const FileTransfer::CheckBookRequest *request,
                        FileTransfer::CheckBookResponse *response,
                        google::protobuf::Closure *done);
  void ReceiveSlice(google::protobuf::RpcController *controller,
                    const FileTransfer::SliceRequest *request,
                    FileTransfer::SliceResponse *response,
                    google::protobuf::Closure *done);
 private:
  boost::shared_ptr<TransferInfo> GetTransferInfoFromConnection(
    const Connection *connection,
    const string &checkbook_dest_filename) const;
  boost::shared_ptr<TransferInfo> GetTransferInfoFromDestName(
    const string &checkbook_dest_filename);
  boost::shared_ptr<TransferInfo> LoadTransferInfoFromDisk(
    const string &checkbook_dest_filename);
  boost::shared_ptr<TransferInfo> GetTransferInfo(
    Connection *connection,
    const string &checkbook_dest_filename);
  bool SaveSliceRequest(const FileTransfer::SliceRequest *slice_request);
  void CloseConnection(const Connection *connection);
  boost::mutex table_mutex_;
  typedef hash_map<string, boost::shared_ptr<TransferInfo> > CheckBookTable;
  typedef hash_map<const Connection*, CheckBookTable> ConnectionToCheckBookTable;
  ConnectionToCheckBookTable connection_table_;
  CheckBookTable check_table_;
  string doc_root_;
};
#endif  // FILE_TRANSFER_SERVICE_HPP_
