#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "glog/logging.h"
#include "server/io_service_pool.hpp"
#include "server/meta.pb.h"
#include "protobuf/service.h"
#include "boost/thread.hpp"
class Connection;
typedef shared_ptr<Connection> ConnectionPtr;
typedef vector<boost::asio::const_buffer> ConstBufferVector;
class Connection : public boost::enable_shared_from_this<Connection>, public Executor {
 public:
  void set_socket(shared_ptr<boost::asio::ip::tcp::socket> socket) {
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

  shared_ptr<boost::asio::ip::tcp::socket> socket() const {
    return socket_;
  }

  void set_io_service(IOServicePtr io_service) {
    io_service_ = io_service;
  }

  virtual ~Connection() {
    VLOG(2) << "Connection Distroy " << this;
    if (IsConnected()) {
      Close();
    }
  }

  virtual void ScheduleRead() = 0;
  virtual ConnectionPtr Clone() = 0;
  virtual void ScheduleWrite() = 0;
  virtual void Close() {
    VLOG(2) << "Connection socket close";
    boost::system::error_code ignored_ec;
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
  }
  virtual bool IsConnected() {
    return socket_.get() && socket_->is_open();
  }
 protected:
  // Handle completion of a read operation.
  virtual void HandleRead(const boost::system::error_code& e,
                  size_t bytes_transferred) = 0;

  // Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e) = 0;
  shared_ptr<boost::asio::io_service> io_service_;
  shared_ptr<boost::asio::ip::tcp::socket> socket_;
  shared_ptr<Executor> executor_;
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

    int status() const {
      return status_;
    }

   private:
    enum InternalStatus {
      IDLE = 0x01,
      READING = 0x01 << 1,
      WRITTING = 0x01 << 2
    };
    int status_;
  };

  // Handle completion of a read operation.
  void HandleRead(const boost::system::error_code& e,
                  size_t bytes_transferred);

  // Handle completion of a write operation.
  void HandleWrite(const boost::system::error_code& e);
  virtual void Handle(shared_ptr<const Decoder> decoder) = 0;

  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;

  Buffer buffer_;

  IOServicePtr io_service_;

  Status status_;

  int incoming_index_;
  boost::mutex incoming_mutex_;
  shared_ptr<vector<SharedConstBuffer> > duplex_[2];
  shared_ptr<Decoder> decoder_;
};

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleRead() {
  VLOG(2) << "connection use count" << shared_from_this().use_count() - 1 << " status: " << status_.status();
  if (status_.reading()) {
    VLOG(2) << "Alreading in reading status";
    return;
  }
  status_.set_reading();
  VLOG(2) << "ScheduleRead" << " socket open: " << socket_->is_open() << " use count " << shared_from_this().use_count() - 1 << " status: " << status_.status() << " " << this;
  VLOG(2) << "connection use count" << shared_from_this().use_count() - 1 << " status: " << status_.status();
  socket_->async_read_some(boost::asio::buffer(buffer_),
      boost::bind(&Connection::HandleRead, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  VLOG(2) << this << " ScheduleRead connection use count" << shared_from_this().use_count() - 1 << " status: " << status_.status();
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleWrite() {
  VLOG(2) << "connection use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() << " " << this;
  if (status_.writting()) {
    VLOG(2) << "Alreading in writting status";
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
  VLOG(2) << "Schedule Write socket open:" << socket_->is_open();
  shared_ptr<vector<SharedConstBuffer> > outcoming(duplex_[outcoming_index]);
  if (outcoming.get() == NULL || outcoming->empty()) {
    status_.clear_writting();
    LOG(WARNING) << "No outcoming";
    return;
  }
  boost::asio::async_write(
      *socket_.get(), *outcoming.get(),
      boost::bind(
          &Connection::HandleWrite, shared_from_this(),
          boost::asio::placeholders::error));
  VLOG(2) << this << " ScheduleWrite connection use count" << shared_from_this().use_count() - 1 << " status: " << status_.status();
}

template <typename Decoder>
void ConnectionImpl<Decoder>::HandleRead(
      const boost::system::error_code& e,
      size_t bytes_transferred) {
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
          << bytes_transferred << " content: "
          << string(buffer_.data(), bytes_transferred);
  VLOG(2) << this << " ScheduleRead handle read connection use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() <<" : " << this;
  if (!e) {
    CHECK(status_.reading());
    boost::tribool result;
    const char *start = buffer_.data();
    const char *end = start + bytes_transferred;
    const char *p = start;
    while (p < end) {
      boost::tie(result, p) =
        decoder_->Decode(p, end);
      if (result) {
        VLOG(2) << "Handle lineformat: size: " << (p - start);
        VLOG(2) << " use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() <<" : " << this;
        shared_ptr<const Decoder> shared_decoder(decoder_);
        decoder_.reset(new Decoder);
        shared_ptr<Executor> this_executor(this->executor());
        VLOG(2) << " use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() <<" : " << this;
        VLOG(2) << " use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() <<" : " << this;
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
        VLOG(2) << this << " ScheduleRead execute use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() <<" : " << this;
      } else if (!result) {
        VLOG(2) << "Parse error";
        status_.clear_reading();
        ScheduleRead();
        break;
      } else {
        VLOG(2) << "Need to read more data";
        status_.clear_reading();
        ScheduleRead();
        return;
      }
    }
    VLOG(2) << this << "ScheduleRead After reach the end use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() <<" : " << this;
    status_.clear_reading();
    ScheduleRead();
  } else {
    status_.clear_reading();
    VLOG(2) << "Read error, clear status and return";
  }
}

template <typename Decoder>
void ConnectionImpl<Decoder>::HandleWrite(const boost::system::error_code& e) {
  VLOG(2) << this << " ScheduleWrite handle write connection use count" << shared_from_this().use_count() - 1 << " status: " << status_.status() << " : " << this;
  CHECK(status_.writting());
  if (!e) {
    status_.clear_writting();
    ScheduleWrite();
  } else {
    status_.clear_writting();
    VLOG(2) << "Write error, clear status and return";
  }
}
#endif // NET2_CONNECTION_HPP_
