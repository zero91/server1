#include "server/timer_master.hpp"

void TimerMaster::Start() {
  CHECK(stop_);
  for (int i = 0; i < arraysize(vecs_); ++i) {
    vecs_[i].index = 0;
  }
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
      TList *l = &vecs_[i].vec[j];
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

void TimerMaster::CascadeTimers(TimerVec *v) {
  TList *l = &v->vec[v->index];
  for (TList::iterator it = l->begin();
       it != l->end();) {
    TimerSlot *s = &*it;
    ++it;
    s->unlink();
    InternalAddTimer(s->weak_timer);
  }
  CHECK(l->empty());
  ++v->index;
}

void TimerMaster::Update(int jiffies) {
  CHECK_GE(jiffies, timer_jiffies_);
  TimerVec *v0 = &vecs_[0];
  while (jiffies - timer_jiffies_ >= 0) {
    boost::mutex::scoped_lock lock(mutex_);
    if (v0->index == 0) {
      int n = 1;
      do {
        CascadeTimers(&vecs_[n]);
      } while (vecs_[n].index == 1 &&  ++n < arraysize(vecs_));
    }
    TList *l = &v0->vec[v0->index];
    for (TList::iterator it = l->begin();
         it != l->end();) {
      scoped_ptr<TimerSlot> s(&*it);
      ++it;
      s->unlink();
      boost::weak_ptr<Timer> weak_timer = s->weak_timer;
      boost::shared_ptr<Timer> timer = weak_timer.lock();
      if (weak_timer.expired()) {
        continue;
      }
      timer->Expired();
      if (timer->period()) {
        InternalAddTimer(weak_timer);
      }
    }
    CHECK(l->empty());
    ++v0->index;
    ++timer_jiffies_;
  }
}

void TimerMaster::InternalAddTimer(
    boost::weak_ptr<Timer> weak_timer) {
  boost::shared_ptr<Timer> timer = weak_timer.lock();
  if (weak_timer.expired()) {
    return;
  }
  int i = 0;
  int idx  = timer->timeout();
  int expires = idx + timer_jiffies_;
  TList *v;
  uint64 upper = 1;
  for (int i = 0; i < arraysize(vecs_); ++i) {
    upper <<= kTVBits;
    if (idx < upper) {
      int j = expires & kTVMask;
      v = &vecs_[i].vec[0] + j;
      break;
    }
    expires >>= kTVBits;
  }
  TimerSlot *slot = new TimerSlot;
  slot->weak_timer = timer;
  v->push_back(*slot);
}

void TimerMaster::Register(boost::weak_ptr<Timer> weak_timer) {
  boost::mutex::scoped_lock locker(mutex_);
  InternalAddTimer(weak_timer);
}
