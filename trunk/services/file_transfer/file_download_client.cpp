#include "services/file_transfer/file_download_client.hpp"

void FileDownloadClient::Start() {
  FileTransfer::RegisterRequest request;
  FileTransfer::RegisterResponse response;
  request.set_src_filename(src_filename);
  request.set_local_filename(local_filename);
  request.set_local_mac_address(GetMacAddress());
  for (int i = 0; i < kConnectionNumber; ++i) {
    const string name("FileDownloadTest2Client." + boost::lexical_cast<string>(i));
    boost::shared_ptr<ClientConnection> r(new ClientConnection(name, FLAGS_address, FLAGS_port));
    boost::shared_ptr<FileTransfer::RegisterResponse> resposne(
        new FileTransfer::RegisterResponse);
    CHECK(!r->IsConnected());
    CHECK(r->Connect());
    r->RegisterService(file_transfer_service_.get());
    r->RegisterService(file_notify_.get());
    connections_.push_back(r);
    responses_.push_back(response);
    FileTransfer::FileDownloadService::Stub stub(r.get());
    request.set_peer_name(r->name());
    stub.RegisterDownload(
        NULL,
        &request, response.get(), NULL);
  }
}

void FileDonwloadClient::Wait() {
  notifier_->Wait();
}
