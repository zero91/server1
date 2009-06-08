#ifndef ATOMIC_HPP_
#define ATOMIC_HPP_
#define atomic_inc __sync_add_and_fetch
#define atomic_dec __sync_sub_and_fetch
#define atomic_compare_and_swap __sync_bool_compare_and_swap
#define atomic_and  __sync_and_and_fetch
#define atomic_or  __sync_or_and_fetch
#define atomic_xor __sync_xor_and_fetch

template <class T>
void intrusive_ptr_add_ref(T *t) {
  atomic_inc(&t->intrusive_count_, 1);
}

template <class T>
void intrusive_ptr_release(T *t) {
  if (atomic_compare_and_swap(&t->intrusive_count_, 1, 0)) {
    delete t;
  } else {
    atomic_dec(&t->intrusive_count_, 1);
  }
}
#endif  // ATOMIC_HPP_

