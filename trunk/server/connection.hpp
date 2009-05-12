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
    VLOG(2) << "Connection close";
    if (IsConnected()) {
      Close();
    }
  }

  virtual void ScheduleRead() = 0;
  virtual ConnectionPtr Clone() = 0;
  virtual void ScheduleWrite() = 0;
  virtual void Close() {
    socket_->close();
  }
  virtual bool IsConnected() {
    return socket_.get() && socket_->is_open();
  }
 protected:
  // Handle completion of a read operation.
  void HandleRead(const boost::system::error_code& e,
                  size_t bytes_transferred,
                  const boost::function2<void, const boost::system::error_code& ,
                  size_t> &call) {
    return call(e, bytes_transferred);
  }

  // Handle completion of a write operation.
  void HandleWrite(const boost::system::error_code& e,
                   const boost::function1<void, const boost::system::error_code &> &call) {
    return call(e);
  }
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
  ConnectionImpl() : incoming_index_(0) {
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

   private:
    enum InternalStatus {
      IDLE = 0x01,
      READING = 0x01 << 1,
      WRITTING = 0x01 << 2
    };
    int status_;
  };

  void InternalHandleRead(
      const boost::system::error_code& e,
      size_t bytes_transferred,
      shared_ptr<Decoder> decoder);

  void InternalHandleWrite(
      const boost::system::error_code& e, shared_ptr<vector<SharedConstBuffer> > o);

  void InternalScheduleRead(
      shared_ptr<Decoder> decoder);

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
  shared_ptr<Decoder> decoder(new Decoder);
  return InternalScheduleRead(decoder);
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalScheduleRead(
    shared_ptr<Decoder> decoder) {
  if (status_.reading()) {
    VLOG(2) << "Alreading in reading status";
    return;
  }
  status_.set_reading();
  VLOG(2) << "ScheduleRead" << " socket open: " << socket_->is_open();
  const boost::function2<void, const boost::system::error_code& , size_t> call(
      boost::bind(&ConnectionImpl<Decoder>::InternalHandleRead, this, _1, _2, decoder));
  socket_->async_read_some(boost::asio::buffer(buffer_),
      boost::bind(&Connection::HandleRead, Connection::shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred,
        call));
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
  const boost::function1<void, const boost::system::error_code &> call(
      boost::bind(&ConnectionImpl<Decoder>::InternalHandleWrite, this, _1, outcoming));
  boost::asio::async_write(
      *socket_.get(), *outcoming.get(),
      boost::bind(
          &Connection::HandleWrite, Connection::shared_from_this(),
          boost::asio::placeholders::error,
          call));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalHandleRead(
      const boost::system::error_code& e,
      size_t bytes_transferred,
      shared_ptr<Decoder> decoder) {
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
          << bytes_transferred << " content: "
          << string(buffer_.data(), bytes_transferred);
  CHECK(status_.reading());
  if (!e) {
    boost::tribool result;
    const char *start = buffer_.data();
    const char *end = start + bytes_transferred;
    const char *p = start;
    while (p < end) {
      boost::tie(result, p) =
        decoder->Decode(p, end);
      if (result) {
        VLOG(2) << "Handle lineformat: size: " << (p - start);
        shared_ptr<const Decoder> shared_decoder(decoder);
        shared_ptr<Executor> this_executor(this->executor());
        if (this_executor.get() == NULL) {
          Handle(shared_decoder);
        } else {
          // This is executed in another thread.
          boost::function0<void> handler = boost::bind(&ConnectionImpl<Decoder>::Handle, this, shared_decoder);
          this_executor->Run(boost::bind(
              &Connection::Run, shared_from_this(), handler));
        }
        decoder.reset(new Decoder);
      } else if (!result) {
        VLOG(2) << "Parse error";
        ScheduleWrite();
        break;
      } else {
        VLOG(2) << "Need to read more data";
        InternalScheduleRead(decoder);
        return;
      }
    }
    status_.clear_reading();
    ScheduleRead();
  } else {
    status_.clear_reading();
  }
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalHandleWrite(
    const boost::system::error_code& e, shared_ptr<vector<SharedConstBuffer> > o) {
  CHECK(status_.writting());
  if (!e) {
    status_.clear_writting();
    ScheduleWrite();
  } else {
    status_.clear_writting();
  }
}
#endif // NET2_CONNECTION_HPP_
