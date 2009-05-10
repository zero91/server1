#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "glog/logging.h"
#include "server/io_service_pool.hpp"
#include "server/meta.pb.h"
#include "protobuf/service.h"
#include "boost/thread.hpp"
class FullDualChannel : virtual public google::protobuf::RpcController,
  virtual public google::protobuf::RpcChannel {
 public:
  virtual bool RegisterService(google::protobuf::Service *service) = 0;
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) = 0;
  void SetFailed(const string &failed) {
    failed_ = failed;
  }
  bool Failed() const {
    return !failed_.empty();
  }
  string ErrorText() const {
    return failed_;
  }
  void StartCancel() {
  }
  bool IsCanceled() const {
    return false;
  }
  void NotifyOnCancel(google::protobuf::Closure *callback) {
  }
  void Reset() {
    failed_.clear();
  }
 private:
  string failed_;
};

class Connection : public boost::enable_shared_from_this<Connection>, public FullDualChannel {
 public:
  void set_socket(shared_ptr<boost::asio::ip::tcp::socket> socket) {
    socket_ = socket;
  }

  shared_ptr<boost::asio::ip::tcp::socket> socket() const {
    return socket_;
  }
  virtual ~Connection() {}

  virtual void ScheduleRead() = 0;
  virtual Connection *Clone() = 0;
  virtual void ScheduleWrite() = 0;
  virtual void Close() {
    socket_->close();
  }
  virtual bool IsConnected() {
    return socket_.get() && socket_->is_open();
  }
 protected:
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred, Object *resource) = 0;
  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e,
                           shared_ptr<Object> resource) = 0;
  shared_ptr<boost::asio::ip::tcp::socket> socket_;
};

typedef shared_ptr<Connection> ConnectionPtr;
// Represents a single Connection from a client.
// The Handler::Handle method should be multi thread safe.
template <typename Decoder>
class ConnectionImpl : virtual public Connection {
 private:
  class SharedConstBuffer : public boost::asio::const_buffer {
   public:
    SharedConstBuffer(const string *data)
      : const_buffer(data->c_str(), data->size()), data_(data) {
    }
   private:
    shared_ptr<const string> data_;
  };
public:
  ConnectionImpl() : incoming_index_(0) {
  }
  Connection *Clone()  = 0;
  // ScheduleRead the first asynchronous operation for the Connection.
  void ScheduleRead();

  void ScheduleWrite();

  template <typename T>
  // The push will take the ownership of the data
  void PushData(const T &data) {
    boost::mutex::scoped_lock locker(incoming_mutex_);
    return InternalPushData(data);
  }

protected:
  template <class T> void InternalPushData(const T &data);
  class Status {
   public:
    Status() : status_(IDLE) {
    }
    bool reading() const {
      return status_ & READING;
    }

    bool writting() const {
      return status_ & WRITTING;
    }

    void set_reading() {
      status_ |= READING;
    }

    void set_writting() {
      status_ |= WRITTING;
    }

    void clear_reading() {
      status_ &= ~READING;
    }

    void clear_writting() {
      status_ &= ~WRITTING;
    }

   private:
    enum InternalStatus {
      IDLE = 0x01,
      READING = 0x01 << 1,
      WRITTING = 0x01 << 2
    };
    int status_;
  };

  /// Handle completion of a read operation.
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred, Object *resource);
  void InternalScheduleRead(Object *resource);

  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e,
                           shared_ptr<Object> resource);

  virtual void Handle(shared_ptr<const Decoder> decoder) = 0;

  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;

  Buffer buffer_;

  IOServicePtr io_service_;

  Status status_;

  int incoming_index_;
  boost::mutex incoming_mutex_;
  shared_ptr<vector<SharedConstBuffer> > duplex_[2];
};

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleRead() {
  return InternalScheduleRead(static_cast<Object*>(0));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalScheduleRead(
    Object *resource) {
  if (status_.reading()) {
    VLOG(2) << "Alreading in reading status";
    return;
  }
  status_.set_reading();
  VLOG(2) << "ScheduleRead" << " socket open: " << socket_->is_open();
  socket_->async_read_some(boost::asio::buffer(buffer_),
      boost::bind(&Connection::HandleRead, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred, resource));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleWrite() {
  if (status_.writting()) {
    VLOG(2) << "Alreading in writting status";
    return;
  }
  status_.set_writting();

  {
    boost::mutex::scoped_lock locker(incoming_mutex_);
    // Switch the working vector.
    incoming_index_ = 1 - incoming_index_;
  }

  const int outcoming_index = 1 - incoming_index_;
  VLOG(2) << "Schedule Write socket open:" << socket_->is_open();
  shared_ptr<vector<SharedConstBuffer> > outcoming(duplex_[outcoming_index]);
  duplex_[outcoming_index].reset();
  if (outcoming.get() == NULL || outcoming->empty()) {
    status_.clear_writting();
    LOG(WARNING) << "No outcoming";
    return;
  }
  boost::asio::async_write(
      *socket_.get(), *outcoming.get(),
      boost::bind(&Connection::HandleWrite, shared_from_this(),
                  boost::asio::placeholders::error,
                  ObjectT<vector<SharedConstBuffer> >::Create(outcoming)));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::HandleRead(
      const boost::system::error_code& e,
      size_t bytes_transferred,
      Object *resource) {
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
          << bytes_transferred << " content: "
          << string(buffer_.data(), bytes_transferred);
  CHECK(status_.reading());
  if (!e) {
    boost::tribool result;
    const char *start = buffer_.data();
    const char *end = start + bytes_transferred;
    const char *p = start;
    Decoder *decoder;
    if (resource == NULL) {
      decoder = new Decoder;
      resource = decoder;
    } else {
      decoder = dynamic_cast<Decoder*>(resource);
    }
    while (p < end) {
      boost::tie(result, p) =
        decoder->Decode(p, end);
      if (result) {
        VLOG(2) << "Handle lineformat: size: " << (p - start);
        shared_ptr<const Decoder> shared_decoder(decoder);
        Handle(shared_decoder);
        ScheduleWrite();
        decoder = new Decoder;
        resource = decoder;
      } else if (!result) {
        VLOG(2) << "Parse error";
        delete resource;
        ScheduleWrite();
        break;
      } else {
        VLOG(2) << "Need to read more data";
        InternalScheduleRead(resource);
        return;
      }
    }
    status_.clear_reading();
    ScheduleRead();
  } else {
    if (resource != NULL) {
      delete resource;
    }
    status_.clear_reading();
  }

  // If an error occurs then no new asynchronous operations are started. This
  // means that all shared_ptr references to the ConnectionImpl object will
  // disappear and the object will be destroyed automatically after this
  // handler returns. The ConnectionImpl class's destructor closes the socket.
}

template <typename Decoder>
void ConnectionImpl<Decoder>::HandleWrite(
      const boost::system::error_code& e, shared_ptr<Object> o) {
    CHECK(status_.writting());
    if (!e) {
      status_.clear_writting();
      ScheduleWrite();
    } else {
      status_.clear_writting();
    }

  // No new asynchronous operations are started. This means that all shared_ptr
  // references to the ConnectionImpl object will disappear and the object will be
  // destroyed automatically after this handler returns. The ConnectionImpl class's
  // destructor closes the socket.
}
#endif // NET2_CONNECTION_HPP_
