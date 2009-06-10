#include "server/timer_master.hpp"

void TimerMaster::Start() {
  CHECK(stop_);
  thread_.reset(new boost::thread(&TimerMaster::InternalRun, this));
}

void TimerMaster::Stop() {
  CHECK(!stop_);
  stop_ = true;
  thread_->join();
  DestroyAllTimers();
}

void TimerMaster::DestroyAllTimers() {
  for (int i = 0; i < arraysize(vecs_); ++i) {
    for (int j = 0; j < kTVSize; ++j) {
      TList *l = &vecs_[i][j];
      for (TList::iterator it = l->begin(); it != l->end();) {
        TimerSlot *s = &*it;
        ++it;
        delete s;
      }
    }
  }
}

void TimerMaster::InternalRun() {
  int jiffiers = 0;
  for (;;) {
    sleep(1);
    Update(++jiffiers);
  }
}

int TimerMaster::CascadeTimers(TimerVec *v, int index) {
  TList *l = &(*v)[index];
  for (TList::iterator it = l->begin();
       it != l->end();) {
    TimerSlot *s = &*it;
    ++it;
    s->unlink();
    InternalAddTimer(s, s->weak_timer, s->jiffies);
  }
  CHECK(l->empty());
  return index;
}

#define INDEX(N) (timer_jiffies_ >> (N * kTVBits)) & kTVMask
void TimerMaster::Update(int jiffies) {
  CHECK_GE(jiffies, timer_jiffies_);
  TimerVec &v0 = vecs_[0];
  while (jiffies - timer_jiffies_ >= 0) {
    boost::mutex::scoped_lock lock(mutex_);
    const int index = timer_jiffies_ & kTVMask;
    if (!index && !CascadeTimers(&vecs_[1], INDEX(1)) &&
        !CascadeTimers(&vecs_[2], INDEX(2))) {
      CascadeTimers(&vecs_[3], INDEX(3));
    }
    TList *l = &v0[index];
    for (TList::iterator it = l->begin();
         it != l->end();) {
      TimerSlot *s = &*it;
      ++it;
      s->unlink();
      boost::weak_ptr<Timer> weak_timer = s->weak_timer;
      boost::shared_ptr<Timer> timer = weak_timer.lock();
      if (weak_timer.expired()) {
        delete s;
        continue;
      }
      timer->Expired();
      if (timer->period()) {
        InternalAddTimer(s, timer, timer->timeout() + timer_jiffies_);
      } else {
        delete s;
      }
    }
    ++timer_jiffies_;
  }
}

void TimerMaster::InternalAddTimer(
    TimerSlot *slot,
    boost::weak_ptr<Timer> weak_timer,
    int jiffies) {
  int i = 0;
  int idx  = jiffies - timer_jiffies_;
  TList *v;
  int expires = jiffies;
  uint64 upper = 1;
  for (int i = 0; i < arraysize(vecs_); ++i) {
    upper <<= kTVBits;
    if (idx < upper) {
      int j = expires & kTVMask;
      v = &vecs_[i][j];
      break;
    }
    expires >>= kTVBits;
  }
  slot->weak_timer = weak_timer;
  slot->jiffies = jiffies;
  v->push_back(*slot);
}

void TimerMaster::Register(boost::weak_ptr<Timer> weak_timer) {
  boost::mutex::scoped_lock locker(mutex_);
  boost::shared_ptr<Timer> timer = weak_timer.lock();
  if (weak_timer.expired()) {
    return;
  }
  TimerSlot *slot = new TimerSlot;
  InternalAddTimer(slot, weak_timer, timer->timeout() + timer_jiffies_);
}
