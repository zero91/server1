#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "base/allocator.hpp"
#include "glog/logging.h"
#include "server/meta.pb.h"
#include "protobuf/service.h"
#include "boost/thread.hpp"
class Connection;
typedef shared_ptr<Connection> ConnectionPtr;
typedef vector<boost::asio::const_buffer> ConstBufferVector;
class ConnectionStatus {
 public:
  ConnectionStatus() : status_(IDLE) {
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

  void set_close() {
    status_ |= CLOSING;
  }

  bool is_closing() const {
    return status_ & CLOSING;
  }

  int status() const {
    return status_;
  }

 private:
  enum InternalConnectionStatus {
    IDLE = 0x01,
    READING = 0x01 << 1,
    WRITTING = 0x01 << 2,
    CLOSING = 0x01 << 3
  };
  int status_;
};

class Connection : public boost::enable_shared_from_this<Connection>, public Executor {
 public:
  void set_socket(boost::asio::ip::tcp::socket *socket) {
    socket_ = socket;
    boost::asio::socket_base::keep_alive option(true);
    socket_->set_option(option);
  }

  void set_executor(shared_ptr<Executor> executor) {
    executor_ = executor;
  }

  shared_ptr<Executor> executor() const {
    return executor_;
  }

  void set_name(const string name) {
    name_ = name;
  }

  const string name() const {
    return name_;
  }

  void set_close_handler(const boost::function0<void> &h) {
    close_handler_ = h;
  }

  void set_allocator(Allocator *allocator) {
    allocator_ = allocator;
  }

  virtual ~Connection() {
    VLOG(2) << name() << " : " << "Connection Distroy " << this;
    if (IsConnected()) {
      Close();
    }
  }

  virtual void ScheduleRead() = 0;
  virtual ConnectionPtr Clone() = 0;
  virtual void ScheduleWrite() = 0;
  virtual void Close() {
    if (status_.is_closing()) {
      VLOG(2) << name() << " : " << "already closing";
      return;
    }
    VLOG(2) << name() << " : " << "Connection socket close";
    boost::system::error_code ignored_ec;
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    if (!close_handler_.empty()) {
      close_handler_();
    }
  }
  virtual bool IsConnected() {
    return socket_ && socket_->is_open();
  }
 protected:
  virtual void HandleRead(const boost::system::error_code& e, size_t bytes_transferred) = 0;
  virtual void HandleWrite(const boost::system::error_code& e, size_t bytes_transferred) = 0;
  boost::asio::ip::tcp::socket* socket_;
  shared_ptr<Executor> executor_;
  string name_;
  boost::function0<void> close_handler_;
  Allocator *allocator_;
  ConnectionStatus status_;
  friend class ConnectionReadHandler;
  friend class ConnectionWriteHandler;
};
class ConnectionReadHandler {
 public:
  ConnectionReadHandler(const ConnectionReadHandler &r) : connection_(r.connection_), allocator_(r.allocator_)  {
    VLOG(2) << "ConnectionReadHandler copy " << this;
  }
  ~ConnectionReadHandler() {
    VLOG(2) << "ConnectionReadHandler destroy " << this;
  }
  ConnectionReadHandler(Connection* connection, Allocator *allocator)
    : connection_(connection), allocator_(allocator) {
  }
  void operator() (const boost::system::error_code &e,
                size_t bytes_transferred) {
    VLOG(2) << "ConnectionReadHandler " << e.message() << " bytes: " << bytes_transferred;
    return connection_->HandleRead(e, bytes_transferred);
  }
  friend void* asio_handler_allocate(std::size_t size,
      ConnectionReadHandler * this_handler) {
    VLOG(2) << "asio_handler_allocate";
    return this_handler->allocator_->Allocate(size);
  }

  friend void asio_handler_deallocate(
      void* pointer, std::size_t /*size*/,
      ConnectionReadHandler* this_handler) {
    VLOG(2) << "asio_handler_deallocate";
    this_handler->allocator_->Deallocate(pointer);
  }
  template <typename F>
  friend void asio_handler_invoke(F f, ConnectionReadHandler *h) {
    VLOG(2) << "asio_handler_invoke";
    f();
  }
 private:
  Connection* connection_;
  Allocator *allocator_;
};

class ConnectionWriteHandler {
 public:
  ConnectionWriteHandler(const ConnectionWriteHandler &r) : connection_(r.connection_), allocator_(r.allocator_) {
    VLOG(2) << "ConnectionWriteHandler copy " << this;
  }
  ~ConnectionWriteHandler() {
    VLOG(2) << "ConnectionWriteHandler destroy " << this;
  }
  ConnectionWriteHandler(Connection* connection, Allocator *allocator)
    : connection_(connection), allocator_(allocator) {
  }
  void operator() (const boost::system::error_code &e,
                size_t bytes_transferred) {
    VLOG(2) << "ConnectionWriteHandler" << e.message() << " bytes: " << bytes_transferred;
    return connection_->HandleWrite(e, bytes_transferred);
  }
  friend void* asio_handler_allocate(std::size_t size,
      ConnectionWriteHandler * this_handler) {
    VLOG(2) << "asio_handler_allocate";
    return this_handler->allocator_->Allocate(size);
  }

  friend void asio_handler_deallocate(
      void* pointer, std::size_t /*size*/,
      ConnectionWriteHandler* this_handler) {
    VLOG(2) << "asio_handler_deallocate";
    this_handler->allocator_->Deallocate(pointer);
  }
  template <typename F>
  void asio_handler_invoke(F f, ConnectionWriteHandler *h) {
    VLOG(2) << "asio_handler_invoke";
    f();
  }
 private:
  Connection* connection_;
  Allocator* allocator_;
};

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
  ConnectionImpl() : Connection(), incoming_index_(0), decoder_(new Decoder) {
  }
  ConnectionPtr Clone()  = 0;
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


  virtual void Handle(shared_ptr<const Decoder> decoder) = 0;
  void HandleRead(const boost::system::error_code& e, size_t bytes_transferred);
  void HandleWrite(const boost::system::error_code& e, size_t bytes_transferred);
  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;

  Buffer buffer_;

  int incoming_index_;
  boost::mutex incoming_mutex_;
  shared_ptr<vector<SharedConstBuffer> > duplex_[2];
  shared_ptr<Decoder> decoder_;
};

template <class Decoder>
void ConnectionImpl<Decoder>::HandleRead(const boost::system::error_code& e,
                                         size_t bytes_transferred) {
  if (!e) {
    VLOG(2) << name() << " : " << this << " ScheduleRead handle read connection";
    VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
      << bytes_transferred << " content: "
      << string(buffer_.data(), bytes_transferred);
    CHECK(status_.reading());
    boost::tribool result;
    const char *start = buffer_.data();
    const char *end = start + bytes_transferred;
    const char *p = start;
    while (p < end) {
      boost::tie(result, p) =
        decoder_->Decode(p, end);
      if (result) {
        VLOG(2) << name() << " : " << "Handle lineformat: size: " << (p - start);
        VLOG(2) << name() << " : " << " use count" << " status: " << status_.status() <<" : " << this;
        shared_ptr<const Decoder> shared_decoder(decoder_);
        decoder_.reset(new Decoder);
        shared_ptr<Executor> this_executor(this->executor());
        VLOG(2) << name() << " : " << " use count" << " status: " << status_.status() <<" : " << this;
        VLOG(2) << name() << " : " << " use count" << " status: " << status_.status() <<" : " << this;
        /*
           if (this_executor.get() == NULL) {
           Handle(shared_decoder);
           } else {
        // This is executed in another thread.
        boost::function0<void> handler = boost::bind(&ConnectionImpl<Decoder>::Handle, this, shared_decoder);
        this_executor->Run(boost::bind(
            &Connection::Run, shared_from_this(), handler));
            }
            */
        Handle(shared_decoder);
        VLOG(2) << name() << " : " << this << " ScheduleRead execute use count" << " status: " << status_.status() <<" : " << this;
      } else if (!result) {
        VLOG(2) << name() << " : " << "Parse error";
        status_.clear_reading();
        ScheduleRead();
        break;
      } else {
        VLOG(2) << name() << " : " << "Need to read more data";
        status_.clear_reading();
        ScheduleRead();
        return;
      }
    }
    VLOG(2) << name() << " : " << this << "ScheduleRead After reach the end use count" << " status: " << status_.status() <<" : " << this;
    status_.clear_reading();
    ScheduleRead();
  } else {
    status_.set_close();
    Close();
    VLOG(2) <<"Read error, clear status and return";
  }
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleRead() {
  VLOG(2) << name() << " : " << "connection use count" << " status: " << status_.status();
  if (status_.reading()) {
    VLOG(2) << name() << " : " << "Alreading in reading status";
    return;
  }
  status_.set_reading();
  VLOG(2) << name() << " : " << "ScheduleRead" << " socket open: " << socket_->is_open() << " use count " << " status: " << status_.status() << " " << this;
  VLOG(2) << name() << " : " << "connection use count" << " status: " << status_.status();
  /*
  socket_->async_read_some(boost::asio::buffer(buffer_),
      boost::bind(::HandleRead, this,
//      boost::bind(&Connection::HandleRead, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  */
  socket_->async_read_some(boost::asio::buffer(buffer_),
                           ConnectionReadHandler(this, allocator_));
  VLOG(2) << name() << " : " << this << " ScheduleRead connection use count" << " status: " << status_.status();
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleWrite() {
  VLOG(2) << name() << " : " << "connection use count" << " status: " << status_.status() << " " << this;
  if (status_.writting()) {
    VLOG(2) << name() << " : " << "Alreading in writting status";
    return;
  }
  status_.set_writting();

  {
    boost::mutex::scoped_lock locker(incoming_mutex_);
    // Switch the working vector.
    incoming_index_ = 1 - incoming_index_;
    duplex_[incoming_index_].reset();
  }

  const int outcoming_index = 1 - incoming_index_;
  VLOG(2) << name() << " : " << "Schedule Write socket open:" << socket_->is_open();
  shared_ptr<vector<SharedConstBuffer> > outcoming(duplex_[outcoming_index]);
  if (outcoming.get() == NULL || outcoming->empty()) {
    status_.clear_writting();
    LOG(WARNING) << "No outcoming";
    return;
  }
  /*
  boost::asio::async_write(
      *socket_.get(), *outcoming.get(),
      boost::bind(
//          &Connection::HandleWrite, shared_from_this(),
          &Connection::HandleWrite, this,
          boost::asio::placeholders::error));
  */
  boost::asio::async_write(
      *socket_, *outcoming.get(),
      ConnectionWriteHandler(this, allocator_));
  VLOG(2) << name() << " : " << this << " ScheduleWrite connection use count" << " status: " << status_.status();
}



template <typename Decoder>
void ConnectionImpl<Decoder>::HandleWrite(const boost::system::error_code& e, size_t bytes_transferred) {
  VLOG(2) << name() << " : " << this << " ScheduleWrite handle write connection use count" << " status: " << status_.status() << " : " << " bytes: " << bytes_transferred;
  CHECK(status_.writting());
  if (!e) {
    status_.clear_writting();
    ScheduleWrite();
  } else {
    status_.clear_writting();
    VLOG(2) << name() << " : " << "Write error, clear status and return";
  }
}
#endif // NET2_CONNECTION_HPP_
