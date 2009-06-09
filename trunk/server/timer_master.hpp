#ifndef TIMER_MASTER_HPP_
#define TIMER_MASTER_HPP_
class Timer : public boost::enable_shared_from_this<Timer> {
 public:
  void Wait(int timeout, boost::function0<void> f);
  bool Cancel();
 private:
  Timer(TimerMaster *master);
  boost::intrusive_link<Timer> TimerLink;
};

class TimerMaster {
 public:
  TimerMaster() : expires_(0) {
  }
  // Update the time slot, bind to a thread.
  void Run();
  void Stop();
  // Call the f after timeout, thread safe.
  void Register(int timeout, boost::weak_ptr<Timer> timer);
 private:
  static const int kTVNBits = 6;
  static const int kTVRBits = 8;
  static const int kTVNSize = (1 << kTVNBits);
  static const int kTVRSize = (1 << kTVRBits);
  static const int kTVNMask = (kTVNSize - 1);
  static const int kTVRMask = (kTVRSize - 1);
  struct TimeT;
  typedef boost::intrusive_link<TimeT> TimeTLink;
  struct TimeT {
    boost::weak_ptr<Timer> timer;
    int expires;
    TimeTLink tlink;
  };
  typedef boost::intrusive_list<TimeT, &TimeT::tlink> TList;
  struct TimerVec {
    int index;
    TList vec[kTVNSize];
  };
  struct TimerVecRoot {
    int index;
    TList vec[kTVRSize];
  };
  int expires_;
};
#endif  // TIMER_MASTER_HPP_
