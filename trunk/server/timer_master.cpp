#include "server/timer_master.hpp"

void TimerMaster::Run() {
  for (;;) {
    sleep(1);
  }
}

void TimerMaster::Update(int jiffies) {
  while (jiffies - timer_jiffies_ >= 0) {
    if (tv1.index == 0) {
      int n = 1;
      do {
        CascadeTimers(tvecs[n]);
      } while (tvecs[n]->index == 1 &&  ++n < kNoofTimeVecs);
    }
  }
}
