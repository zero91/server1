#include "services/file_transfer/checkbook.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include "server/connection.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <zlib.h>
#include <glog/logging.h>
static const boost::shared_ptr<TransferInfo> kEmptyTransferInfo;
static string GetSliceName(const FileTransfer::Slice *slice) {
 return slice->checkbook_dest_filename() + "." + boost::lexical_cast<string>(slice->index());
}

struct TransferInfo : public boost::enable_shared_from_this<TransferInfo> {
  mutable int connection_count;
  string checkbook_dest_filename;
  boost::shared_ptr<CheckBook> checkbook;
  boost::mutex save_mutex;
  static boost::shared_ptr<TransferInfo> Load(const string &in_checkbook_dest_filename) {
    boost::shared_ptr<TransferInfo> transfer_info(new TransferInfo);
    transfer_info->checkbook.reset(CheckBook::Load(in_checkbook_dest_filename));
    if (transfer_info->checkbook.get() == NULL) {
      return kEmptyTransferInfo;
    }
    transfer_info->connection_count = 0;
    transfer_info->checkbook_dest_filename = in_checkbook_dest_filename;
    return transfer_info;
  }

  bool Save() {
    boost::mutex::scoped_lock locker(save_mutex);
    checkbook->Save(checkbook->GetCheckBookDestFileName());
    int i;
    const int slice_size = checkbook->slice_size();
    for (i = 0; i < slice_size; ++i) {
      if (!checkbook->slice(i).finished()) {
        break;
      }
    }
    if (i < checkbook->slice_size()) {
      return false;
    }
    const FileTransfer::Slice &last_slice = checkbook->slice(slice_size - 1);
    int file_size = last_slice.offset() + last_slice.length();
    const string dest_filename = checkbook->meta().dest_filename();
    boost::iostreams::mapped_file_params p(dest_filename);
    p.mode = std::ios_base::out | std::ios_base::trunc;
    p.new_file_size = file_size;
    boost::iostreams::mapped_file out;
    out.open(p);
    if (out.is_open()) {
      LOG(WARNING) << "Fail to open file " << dest_filename;
      return false;
    }
    for (int i = 0; i < slice_size; ++i) {
      const FileTransfer::Slice &slice = checkbook->slice(i);
      const string slice_name = GetSliceName(&slice);
      FileTransfer::SliceRequest slice_request;
      if (!boost::filesystem::exists(slice_name)) {
        LOG(WARNING) << "Slice file: " << slice_name << " don't existing";
        return false;
      }
      ifstream in(slice_name.c_str(), ios::in | ios::binary);
      if (!slice_request.ParseFromIstream(&in)) {
        LOG(WARNING) << "Fail to parse slice file: " << slice_name;
        return false;
      }
      const string &content = slice_request.content();
      uint32 adler;
      adler = adler32(slice.previous_adler(),
                      reinterpret_cast<const Bytef*>(content.c_str()),
                      content.size());
      if (adler != slice.adler()) {
        LOG(WARNING) << "slice: " << slice.index() << " checksum error";
        return false;
      }
      memmove(out.data() + slice.offset(),
              content.c_str(),
              slice.length());
    }
    return true;
  }
};


bool FileTransferServiceImpl::SaveSliceRequest(
    const FileTransfer::SliceRequest *slice_request) {
  const FileTransfer::Slice &slice = slice_request->slice();
  const string &content = slice_request->content();
  uint32 adler;
  adler = adler32(slice.previous_adler(),
                  reinterpret_cast<const Bytef*>(content.c_str()),
                  content.size());
  if (adler != slice.adler()) {
    LOG(WARNING) << "slice: " << slice.index() << " checksum error";
    return false;
  }

  const string filename = GetSliceName(&slice_request->slice());
  ofstream output(filename.c_str(), ios::out | ios::trunc | ios::binary);
  if (!slice_request->SerializeToOstream(&output)) {
    LOG(WARNING) << "Failed to save the CheckBook to:" << filename
                 << " error: " << strerror(errno);
    return false;
  }
  return true;
}

void FileTransferServiceImpl::ReceiveCheckBook(
    google::protobuf::RpcController *controller,
    const FileTransfer::CheckBookRequest *request,
    FileTransfer::CheckBookResponse *response,
    google::protobuf::Closure *done) {
  const FileTransfer::CheckBook &checkbook = request->checkbook();
  const string &dest_checkbook_filename = CheckBook::GetCheckBookDestFileName(
      &checkbook.meta());
  CheckBook::Save(&checkbook, dest_checkbook_filename);
  response->set_succeed(true);
  done->Run();
}

boost::shared_ptr<TransferInfo> FileTransferServiceImpl::GetTransferInfoFromConnection(
    const Connection *connection,
    const string &checkbook_dest_filename) const {
  ConnectionToCheckBookTable::const_iterator it = connection_table_.find(
      connection);
  if (it == connection_table_.end()) {
    return kEmptyTransferInfo;
  }
  const CheckBookTable &check_table = it->second;
  CheckBookTable::const_iterator jt = check_table.find(checkbook_dest_filename);
  if (jt == check_table.end()) {
    return kEmptyTransferInfo;
  }
  return jt->second;
}

boost::shared_ptr<TransferInfo> FileTransferServiceImpl::GetTransferInfoFromDestName(
    const string &checkbook_dest_filename) {
  CheckBookTable::const_iterator jt = check_table_.find(checkbook_dest_filename);
  if (jt == check_table_.end()) {
    return kEmptyTransferInfo;
  }
  return jt->second;
}

boost::shared_ptr<TransferInfo> FileTransferServiceImpl::LoadTransferInfoFromDisk(
    const string &checkbook_dest_filename) {
  return TransferInfo::Load(checkbook_dest_filename);
}

boost::shared_ptr<TransferInfo> FileTransferServiceImpl::GetTransferInfo(
    Connection *connection,
    const string &checkbook_dest_filename) {
  boost::mutex::scoped_lock locker(table_mutex_);
  boost::shared_ptr<TransferInfo> task_info = GetTransferInfoFromConnection(
      connection,
      checkbook_dest_filename);
  if (task_info.get() == NULL) {
    task_info = GetTransferInfoFromDestName(checkbook_dest_filename);
    if (task_info.get() == NULL) {
      task_info = LoadTransferInfoFromDisk(checkbook_dest_filename);
      if (task_info == NULL) {
        return kEmptyTransferInfo;
      } else {
        connection_table_[connection][task_info->checkbook_dest_filename] = task_info;
        check_table_[task_info->checkbook_dest_filename] = task_info;
        ++task_info->connection_count;
        connection->push_close_handler(boost::bind(
            &FileTransferServiceImpl::CloseConnection, this, connection));
      }
    } else {
      connection_table_[connection][task_info->checkbook_dest_filename] = task_info;
      ++task_info->connection_count;
      connection->push_close_handler(boost::bind(
          &FileTransferServiceImpl::CloseConnection, this, connection));
    }
  }
  return task_info;
}

void FileTransferServiceImpl::ReceiveSlice(
    google::protobuf::RpcController *controller,
    const FileTransfer::SliceRequest *request,
    FileTransfer::SliceResponse *response,
    google::protobuf::Closure *done) {
  Connection *connection = dynamic_cast<Connection*>(controller);
  if (connection == NULL) {
    LOG(WARNING) << "fail to convert controller to connection!";
    response->set_succeed(false);
    done->Run();
    return;
  }
  const string checkbook_dest_filename = request->slice().checkbook_dest_filename();
  boost::shared_ptr<TransferInfo> transfer_info = GetTransferInfo(connection, checkbook_dest_filename);
  if (!SaveSliceRequest(request)) {
    LOG(WARNING) << "Fail to save slice: " << request->slice().index();
    response->set_succeed(false);
    done->Run();
    return;
  }
  transfer_info->checkbook->mutable_slice(request->slice().index())->set_finished(true);
  if (transfer_info->Save()) {
    response->set_finished(true);
  }
  response->set_succeed(true);
  done->Run();
}

void FileTransferServiceImpl::CloseConnection(
    const Connection *connection) {
  boost::mutex::scoped_lock locker(table_mutex_);
  for (ConnectionToCheckBookTable::iterator it =
       connection_table_.begin();
       it != connection_table_.end();) {
    ConnectionToCheckBookTable::iterator next_it = it;
    ++next_it;
    CheckBookTable *checkbook_table = &it->second;
    for (CheckBookTable::iterator jt = checkbook_table->begin();
         jt != checkbook_table->end();) {
      CheckBookTable::iterator next_jt = jt;
      ++next_jt;
      boost::shared_ptr<TransferInfo> transfer_info = jt->second;
      --transfer_info->connection_count;
      if (transfer_info->connection_count == 0) {
        checkbook_table->erase(jt);
        check_table_.erase(transfer_info->checkbook_dest_filename);
        boost::shared_ptr<CheckBook> checkbook = transfer_info->checkbook;
        const string &dest_checkbook_filename = CheckBook::GetCheckBookDestFileName(
            &checkbook->meta());
        CheckBook::Save(checkbook.get(), dest_checkbook_filename);
      }
      jt = next_jt;
    }
    if (checkbook_table->empty()) {
      connection_table_.erase(it);
    }
    it = next_it;
  }
}
