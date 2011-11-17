/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */
#ifndef TIMER_MASTER_HPP_
#define TIMER_MASTER_HPP_
#include "base/base.hpp"
#include "boost/intrusive/list.hpp"
#include "boost/thread.hpp"
#include "server/timer.hpp"
class TimerMaster {
 public:
  TimerMaster() : timer_jiffies_(1), stop_(true) {
  }
  // Update the time slot, bind to a thread.
  void Start();
  void Stop();
  bool IsRunning() const {
    return !stop_;
  }
  // Call the f after timeout, thread safe.
  void Register(boost::weak_ptr<Timer> weak_timer);
  void Update(int jiffies);
  ~TimerMaster() {
    if (!stop_) {
      Stop();
    }
    {
      boost::mutex::scoped_lock lock(mutex_);
      DestroyAllTimers();
    }
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
  volatile bool stop_;
};
#endif  // TIMER_MASTER_HPP_
