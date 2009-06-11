#ifndef TIMER_HPP_
#define TIMER_HPP_
class Timer {
 public:
  virtual bool period() const = 0;
  virtual int timeout() const = 0;
  virtual void Expired() = 0;
};
#endif  // TIMER_HPP_
