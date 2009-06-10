#ifndef TIMER_MASTER_HPP_
#define TIMER_MASTER_HPP_
#include "base/base.hpp"
#include "boost/intrusive/list.hpp"
#include "boost/thread.hpp"
class Timer {
 public:
  virtual bool period() const = 0;
  virtual int timeout() const = 0;
  virtual void Expired() = 0;
};

class TimerMaster {
 public:
  TimerMaster() : timer_jiffies_(1), stop_(true) {
  }
  // Update the time slot, bind to a thread.
  void Start();
  void Stop();
  // Call the f after timeout, thread safe.
  void Register(boost::weak_ptr<Timer> weak_timer);
  void Update(int jiffies);
  ~TimerMaster() {
    DestroyAllTimers();
  }
 protected:
  static const int kTVBits = 8;
  static const int kTVSize = (1 << kTVBits);
  static const int kTVMask = (kTVSize - 1);
  typedef boost::intrusive::list_base_hook<
    boost::intrusive::link_mode<
    boost::intrusive::auto_unlink> > TimerSlotBaseHook;
  struct TimerSlot : public TimerSlotBaseHook {
    boost::weak_ptr<Timer> weak_timer;
    int jiffies;
  };
  typedef boost::intrusive::list<TimerSlot,
          boost::intrusive::constant_time_size<false> > TList;
  typedef TList TimerVec[kTVSize];
  void DestroyAllTimers();
  void InternalRun();
  void InternalAddTimer(TimerSlot *slot,
                        boost::weak_ptr<Timer> timer, int jiffies);
  int CascadeTimers(TimerVec *v, int index);
  int timer_jiffies_;
  TimerVec vecs_[4];
  boost::mutex mutex_;
  scoped_ptr<boost::thread> thread_;
  bool stop_;
};
#endif  // TIMER_MASTER_HPP_
