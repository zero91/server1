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
#ifndef FILE_TRANSFER_SERVICE_HPP_
#define FILE_TRANSFER_SERVICE_HPP_
#include "base/base.hpp"
#include "base/hash.hpp"
#include "thread/threadpool.hpp"
#include <boost/thread/mutex.hpp>
#include "server/protobuf_connection.hpp"
#include "services/file_transfer/checkbook.hpp"
#include "services/file_transfer/file_transfer.pb.h"
class FileTransferClient;
class TransferInfo;
class Connection;
class FileTransferServiceImpl :
  public FileTransfer::FileTransferService,
  public Connection::AsyncCloseListener,
  public boost::enable_shared_from_this<FileTransferServiceImpl> {
 public:
  FileTransferServiceImpl(const string &doc_root) : doc_root_(doc_root) {
  }
  void ReceiveCheckBook(google::protobuf::RpcController *controller,
                        const FileTransfer::CheckBook *request,
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
  void ConnectionClosed(Connection *connection);
  boost::mutex table_mutex_;
  typedef hash_map<string, boost::shared_ptr<TransferInfo> > CheckBookTable;
  typedef hash_map<const Connection*, CheckBookTable> ConnectionToCheckBookTable;
  ConnectionToCheckBookTable connection_table_;
  CheckBookTable check_table_;
  string doc_root_;
};
#endif  // FILE_TRANSFER_SERVICE_HPP_
