#ifndef SHARED_CONST_BUFFERS_HPP_
#define SHARED_CONST_BUFFERS_HPP_
#include "base/base.hpp"
#include <boost/asio.hpp>
class SharedConstBuffers {
 private:
  struct Store {
    vector<const string *> data;
    ~Store() {
      for (int i = 0; i < data.size(); ++i) {
        VLOG(2) << "SharedConstBuffers Store delete: " << data[i];
        delete data[i];
      }
      data.clear();
    }
  };
 public:
  // Implement the ConstBufferSequence requirements.
  typedef boost::asio::const_buffer value_type;
  typedef vector<boost::asio::const_buffer>::const_iterator const_iterator;
  const const_iterator begin() const {
    return buffer_.begin() + start_;
  }
  const const_iterator end() const {
    return buffer_.end();
  }
  SharedConstBuffers() : start_(0), store_(new Store) {
  }
  ~SharedConstBuffers() {
    clear();
  }
  void push(const string *data) {
    VLOG(2) << "SharedConstBuffers push: " << data;
    store_->data.push_back(data);
    buffer_.push_back(boost::asio::const_buffer(data->c_str(), data->size()));
  }
  void clear() {
    VLOG(2) << "Clear SharedConstBuffers";
    buffer_.clear();
    store_.reset(new Store);
    start_ = 0;
  }
  bool empty() const {
    return (start_ == buffer_.size());
  }
  void consume(int size) {
    for (int i = start_; i < buffer_.size(); ++i) {
      const int bsize = boost::asio::buffer_size(buffer_[i]);
      if (size > bsize) {
        size -= bsize;
      } else if (size == bsize) {
        start_ = i + 1;
        return;
      } else {
        buffer_[i] = buffer_[i] + size;
        start_ = i;
        return;
      }
    }
    start_ = buffer_.size();
  }
 private:
  vector<boost::asio::const_buffer> buffer_;
  boost::shared_ptr<Store> store_;
  int start_;
};
#endif // SHARED_CONST_BUFFERS_HPP_
