#include "services/file_transfer/checkbook.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include "server/connection.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <zlib.h>
#include <glog/logging.h>
#include <errno.h>
static const boost::shared_ptr<TransferInfo> kEmptyTransferInfo;
static string GetSliceName(const FileTransfer::Slice *slice) {
 return slice->checkbook_dest_filename() + "." + boost::lexical_cast<string>(slice->index());
}

class TransferInfo : public boost::enable_shared_from_this<TransferInfo> {
 public:
  static boost::shared_ptr<TransferInfo> Load(const string &checkbook_dest_filename) {
    boost::shared_ptr<TransferInfo> transfer_info(new TransferInfo);
    transfer_info->checkbook_.reset(CheckBook::Load(checkbook_dest_filename));
    if (transfer_info->checkbook_.get() == NULL) {
      return kEmptyTransferInfo;
    }
    transfer_info->saved_ = false;
    transfer_info->connection_count_ = 0;
    return transfer_info;
  }

  void set_slice_finished(int index) {
    checkbook_->mutable_slice(index)->set_finished(true);
  }
  void SaveCheckBook(const string &doc_root);
  bool Save(const string &doc_root);
  void inc_connection_count() {
    ++connection_count_;
  }
  void dec_connection_count() {
    --connection_count_;
  }
  int connection_count() const {
    return connection_count_;
  }
  const string &checkbook_dest_filename() const {
    return checkbook_->meta().checkbook_dest_filename();
  }
 private:
  TransferInfo() {
  }
  mutable int connection_count_;
  boost::shared_ptr<CheckBook> checkbook_;
  boost::mutex save_mutex_;
  bool saved_;
};

void TransferInfo::SaveCheckBook(const string &doc_root) {
  boost::mutex::scoped_lock locker(save_mutex_);
  if (saved_) {
    return;
  }
  boost::filesystem::path checkbook_dest_filename(doc_root);
  checkbook_dest_filename /= this->checkbook_dest_filename();
  checkbook_->Save(checkbook_dest_filename.file_string());
  VLOG(2) << "SaveCheckbook to: " << checkbook_dest_filename.file_string();
}

bool TransferInfo::Save(const string &doc_root) {
  boost::mutex::scoped_lock locker(save_mutex_);
  if (saved_) {
    return true;
  }
  int i;
  const int slice_size = checkbook_->slice_size();
  for (i = 0; i < slice_size; ++i) {
    if (!checkbook_->slice(i).finished()) {
      break;
    }
  }
  boost::filesystem::path checkbook_dest_filename(doc_root);
  checkbook_dest_filename /= this->checkbook_dest_filename();
  if (i < slice_size) {
    VLOG(2) << "i: " << i << " total slice: " << slice_size << " unfinished";
    return false;
  }
  VLOG(2) << this->checkbook_dest_filename() << " transfer finished";
  const FileTransfer::Slice &last_slice = checkbook_->slice(slice_size - 1);
  int file_size = last_slice.offset() + last_slice.length();
  boost::filesystem::path dest_filename(doc_root);
  dest_filename /= checkbook_->meta().dest_filename();
  boost::iostreams::mapped_file_params p(dest_filename.file_string());
  p.mode = std::ios_base::out | std::ios_base::trunc;
  p.new_file_size = file_size;
  boost::iostreams::mapped_file out;
  out.open(p);
  if (!out.is_open()) {
    LOG(WARNING) << "Fail to open file " << dest_filename << " error: " << strerror(errno);
    return false;
  }
  for (int i = 0; i < slice_size; ++i) {
    const FileTransfer::Slice &slice = checkbook_->slice(i);
    boost::filesystem::path slice_name(doc_root);
    slice_name /= GetSliceName(&slice);
    FileTransfer::SliceRequest slice_request;
    if (!boost::filesystem::exists(slice_name)) {
      LOG(WARNING) << "Slice file: " << slice_name << " don't existing";
      return false;
    }
    ifstream in(slice_name.file_string().c_str(), ios::in | ios::binary);
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
  out.close();
  for (int i = 0; i < slice_size; ++i) {
    const FileTransfer::Slice &slice = checkbook_->slice(i);
    boost::filesystem::path slice_name(doc_root);
    slice_name /= GetSliceName(&slice);
    boost::filesystem::remove(slice_name);
  }
  boost::filesystem::remove(checkbook_dest_filename);
  saved_ = true;
  return true;
}

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

  boost::filesystem::path filename(doc_root_);
  filename /= GetSliceName(&slice_request->slice());
  ofstream output(filename.file_string().c_str(), ios::out | ios::trunc | ios::binary);
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
  boost::filesystem::path dest_checkbook_filename(doc_root_);
  dest_checkbook_filename /= CheckBook::GetCheckBookDestFileName(&checkbook.meta());
  VLOG(2) << "Receive checkbook: " << dest_checkbook_filename;
  CheckBook::Save(&checkbook, dest_checkbook_filename.file_string());
  response->set_succeed(true);
  done->Run();
}

boost::shared_ptr<TransferInfo> FileTransferServiceImpl::GetTransferInfoFromConnection(
    const Connection *connection,
    const string &checkbook_dest_filename) const {
  ConnectionToCheckBookTable::const_iterator it = connection_table_.find(
      connection);
  if (it == connection_table_.end()) {
    VLOG(2) << "Can't find " << connection << " from connection table";
    return kEmptyTransferInfo;
  }
  VLOG(2) << "Find " << connection << " from connection table";
  const CheckBookTable &check_table = it->second;
  CheckBookTable::const_iterator jt = check_table.find(checkbook_dest_filename);
  if (jt == check_table.end()) {
    VLOG(2) << "Can't find " << checkbook_dest_filename << " from checkbook table";
    return kEmptyTransferInfo;
  }
  VLOG(2) << "Find " << connection << " : " << checkbook_dest_filename << " from connection table";
  return jt->second;
}

boost::shared_ptr<TransferInfo> FileTransferServiceImpl::GetTransferInfoFromDestName(
    const string &checkbook_dest_filename) {
  CheckBookTable::const_iterator jt = check_table_.find(checkbook_dest_filename);
  if (jt == check_table_.end()) {
    VLOG(2) << "Can't find " << checkbook_dest_filename << " from dest name table";
    return kEmptyTransferInfo;
  }
  VLOG(2) << "Find " << checkbook_dest_filename << " from dest name table";
  return jt->second;
}

boost::shared_ptr<TransferInfo> FileTransferServiceImpl::LoadTransferInfoFromDisk(
    const string &checkbook_dest_filename) {
  boost::filesystem::path path(doc_root_);
  path /= checkbook_dest_filename;
  return TransferInfo::Load(path.file_string());
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
        VLOG(2) << "Can't GetTransferInfo";
        return kEmptyTransferInfo;
      } else {
        VLOG(2) << "GetTransferInfo from disk";
        const string &checkbook_dest_filename = task_info->checkbook_dest_filename();
        VLOG(2) << "Insert : " << connection << " : " << checkbook_dest_filename << " to connection table";
        connection_table_[connection][checkbook_dest_filename] = task_info;
        check_table_[checkbook_dest_filename] = task_info;
        VLOG(2) << "Insert : " << checkbook_dest_filename << " to dest name table";
        task_info->inc_connection_count();
        connection->push_close_handler(boost::bind(
            &FileTransferServiceImpl::CloseConnection, this, connection));
      }
    } else {
      VLOG(2) << "GetTransferInfo from Destname table";
      const string &checkbook_dest_filename = task_info->checkbook_dest_filename();
      connection_table_[connection][checkbook_dest_filename] = task_info;
      VLOG(2) << "Insert : " << connection << " : " << checkbook_dest_filename << " to connection table";
      task_info->inc_connection_count();
      connection->push_close_handler(boost::bind(
          &FileTransferServiceImpl::CloseConnection, this, connection));
    }
  } else {
    VLOG(2) << "GetTransferInfo from ConnectionTable";
  }
  return task_info;
}

void FileTransferServiceImpl::ReceiveSlice(
    google::protobuf::RpcController *controller,
    const FileTransfer::SliceRequest *request,
    FileTransfer::SliceResponse *response,
    google::protobuf::Closure *done) {
  VLOG(2) << "Receive slice: " << request->slice().index();
  Connection *connection = dynamic_cast<Connection*>(controller);
  if (connection == NULL) {
    LOG(WARNING) << "fail to convert controller to connection!";
    response->set_succeed(false);
    done->Run();
    return;
  }
  const string checkbook_dest_filename = request->slice().checkbook_dest_filename();
  VLOG(2) << "Receive slice, checkbook: " << checkbook_dest_filename;
  boost::shared_ptr<TransferInfo> transfer_info = GetTransferInfo(connection, checkbook_dest_filename);
  if (transfer_info.get() == NULL) {
    response->set_succeed(false);
    LOG(WARNING) << "Fail to get checkbook for slice: " << request->slice().index();
    done->Run();
    return;
  }

  if (!SaveSliceRequest(request)) {
    LOG(WARNING) << "Fail to save slice: " << request->slice().index();
    response->set_succeed(false);
    done->Run();
    return;
  }
  transfer_info->set_slice_finished(request->slice().index());
  if (transfer_info->Save(doc_root_)) {
    response->set_finished(true);
  }
  response->set_succeed(true);
  done->Run();
}

void FileTransferServiceImpl::CloseConnection(
    const Connection *connection) {
  VLOG(2) << "CloseConnection: " << connection;
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
      transfer_info->dec_connection_count();
      if (transfer_info->connection_count() == 0) {
        VLOG(2) << transfer_info->checkbook_dest_filename()
                << " have zero connection, flush the checkbook";
        checkbook_table->erase(jt);
        check_table_.erase(transfer_info->checkbook_dest_filename());
        transfer_info->SaveCheckBook(doc_root_);
      }
      jt = next_jt;
    }
    if (checkbook_table->empty()) {
      VLOG(2) << connection << " have null checkbook table, removed";
      connection_table_.erase(it);
    }
    it = next_it;
  }
}