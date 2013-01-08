/* -*- Mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; compile-command: "make -j2 test" -*- */
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <pthread.h>
#include <atomic>
#include <stdexcept>
#include <assert.h>
#include <boost/shared_ptr.hpp>


//#include <relacy/relacy_std.hpp>

#define CACHE_LINE 64
namespace nanahan {
namespace detail {

template <typename T>
class lfstack {
  typedef lfstack<T> lfstack_t;
  struct node {
    T item_;
    std::atomic<node*> next_;
    node(const T& item):item_(item) {}
  };
  struct head_ptr {
    node* ptr_;
    uint64_t cnt_;
    head_ptr(node* ptr, uint64_t cnt):ptr_(ptr), cnt_(cnt) {}
  };
public:
  lfstack():head_(head_ptr(NULL, 0)){}
  void delete_all() {
    node* ptr = head_.load().ptr_;
    while(ptr != NULL) {
      node* next = ptr->next_.load();
      delete ptr;
      ptr = next;
    }
  }
  void push(const T& item) {
    node* new_node = new node(item);
    for (;;) {
      head_ptr old_head = head_.load();
      new_node->next_ = old_head.ptr_;
      if(head_.compare_exchange_strong(old_head,
                                       head_ptr(new_node, old_head.cnt_+1))) {
        return;
      }
    }
  }
  T pop() {
    node* next_node;
    for (;;) {
      head_ptr old_head = head_.load();
      if (old_head.ptr_ == NULL) {
        throw std::logic_error("empty");
      }
      next_node = old_head.ptr_;
      T got_value = old_head.ptr_->item_;
      if(head_.compare_exchange_strong(old_head,
                                       head_ptr(next_node, old_head.cnt_+1))) {
        delete old_head.ptr_;
        return got_value;
      }
    }
  }
  std::atomic<head_ptr> head_;
};

template <typename T>
class memory_pool {
public:
  memory_pool(){};
  template <typename t1>
  T* alloc(const t1& a1) {
    try {
      T* candidate = inner_pool_.pop();
    }
    catch (const std::logic_error& e){
      return new T(a1);
    }
  }

  void dealloc(T* target) {
    inner_pool_.push(target);
  }

  ~memory_pool(){
    for (;;) {
      try {
        T* ptr = inner_pool_.pop();
        delete ptr;
      } catch (const std::logic_error& e) {
        break; // do until empty
      }
    }
  }
private:
  lfstack<T*> inner_pool_;
};
} // detail

template <typename T>
bool compare_and_swap(T** target, T* expected, T* desired) {
  return __sync_bool_compare_and_swap_8(target,
                                 reinterpret_cast<uint64_t>(expected),
                                 reinterpret_cast<uint64_t>(desired));
}
bool compare_and_swap(uint64_t* target, uint64_t expected, uint64_t desired) {
  return __sync_bool_compare_and_swap_8(target,
                                        expected,
                                        desired);
}
#define mb() asm volatile("":::"memory");


__thread int my_index = 0;

/*
  concurrent wait-free stack
 */
template <typename T>
class Stack {
  typedef Stack<T> stack_t;
  struct node {
    T item_;
    std::atomic<node*> next_;
    std::atomic<int> pusher_;
    std::atomic<int> popper_;
    node(const T& item)
      :item_(item),next_(NULL), pusher_(-1), popper_(-1)
    {}
    friend std::ostream& operator<<(std::ostream& os, const node& n) {
      os << "[item:" << n.item_
         << " next_:" << n.next_.load()
         << " pusher:" << n.pusher_.load()
         << " popper:" << n.popper_.load() << "]";
      return os;
    }
  };

  struct state {
    std::atomic<uint64_t> phase_;
    std::atomic<bool> pending_;
    bool push_;
    node* node_;
    friend std::ostream& operator<<(std::ostream& os, const state& s) {
      os << "[phase:" << s.phase_
         << " pending:" << s.pending_.load()
         << " push:" << ((s.push_) ? "push" : "pop")
         << " node:" << s.node_ << "]";
      return os;
    }
    bool is_pending() {
      return pending_.load();
    }
    void make_pending() {
      bool _false = false;
      bool _true = true;
      pending_.compare_exchange_strong(_false, _true);
      //pending_.store(true);
    }
    void make_unpend() {
      bool _false = false;
      bool _true = true;
      pending_.compare_exchange_strong(_true, _false);
      //pending_.store(false);
    }
    state():phase_(0),pending_(false),push_(true),node_(NULL){}
  } __attribute__((aligned(CACHE_LINE)));
public:
  Stack(int m, std::ostream& os):head_(NULL), max_threads_(m),threads_(0), output_(os) {
    state* new_states = new state[max_threads_ + 1];
    for (int i = 0; i <= max_threads_; ++i) {
      new_states[i].phase_.store(0);
      new_states[i].make_unpend();
      new_states[i].push_ = false;
      new_states[i].node_ = NULL;
    }
    states_.store(new_states, std::memory_order_release);
  }
  void prepare() {
    while(my_index == 0) {
      my_index = __sync_fetch_and_add(&threads_, 1);
    }
  }
  void push(const T& item) {
    const uint64_t my_phase = max_phase() + 1;
    node* const new_node = node_pool_.alloc(item);
    states_[my_index].phase_.store(my_phase);
    states_[my_index].push_ = true;
    states_[my_index].node_ = new_node;

    states_[my_index].make_pending();

    help(my_phase);
    finish();
    assert(!states_[my_index].is_pending());
  }

  T pop() {
    const uint64_t my_phase = max_phase() + 1;
    states_[my_index].push_ = false;
    states_[my_index].node_ = NULL;
    states_[my_index].phase_.store(my_phase);

    states_[my_index].make_pending();

    help(my_phase);
    finish();
    assert(!states_[my_index].is_pending());
    if (states_[my_index].node_ == NULL) {
      std::cerr << "error: stack empty" << std::endl;
      exit(1);
      throw std::length_error("stack is empty");
    }
    T result(states_[my_index].node_->item_);

    delete states_[my_index].node_;
    return result;
  }


  bool is_still_pending(int tid, uint64_t phase) {
    return states_[tid].is_pending() && (states_[tid].phase_.load() <= phase);
  }

  void help(uint64_t phase) {
    int tid;
    while((tid = scan(phase)) != -1) {
      if (states_[tid].push_) {
        help_push(tid, phase);
      } else { // pop
        help_pop(tid, phase);
      }
    }
  }

  void help_push(int tid, uint64_t phase) {
    do {
      node* old_head = head_.load();
      node* pushing = states_[tid].node_;
      node* old_next = pushing->next_.load();
      if (old_head != head_.load()) { continue; }
      if (old_head == NULL) {
        uint64_t old_phase = states_[tid].phase_.load();
        if (is_still_pending(tid, phase)) {
          if (head_.compare_exchange_strong(old_head, pushing)) {
            states_[tid].make_unpend();
            break;
          }
        }
        continue;
      } else {
        int old_pusher = old_head->pusher_.load();
        int old_popper = old_head->popper_.load();
        if (old_head != head_.load()) { continue; }
        if (old_popper != -1 || old_pusher != -1) { // some thread moving
          finish();
        } else {
          if (is_still_pending(tid, phase)) {
            int expect = -1;
            if (old_head->pusher_.compare_exchange_strong(expect, tid)) {
              finish();
            }
          }
        }
      }
    } while (false);
  }
  void help_pop(int tid, uint64_t phase){
    do {
      node* old_head = head_.load();
      if (old_head == NULL) { // linelize point
        if (is_still_pending(tid, phase)) {
          std::cerr << "emptyyyyyy!" << std::endl;
          states_[tid].make_unpend();
          return;
        }
      }
      node* old_next = old_head->next_.load();
      int popper = old_head->popper_.load();
      int pusher = old_head->pusher_.load();
      if (old_head != head_.load()) { continue; }
      if (popper != -1 || pusher != -1) { finish(); }
      else {
        if (is_still_pending(tid, phase)) {
          if (head_.load()->popper_.compare_exchange_strong(popper, tid)) {
            finish();
          }
        }
      }
    } while(false);
  }
  void finish() {
    // get certified thread and ensure finish it
    node* old_head = head_.load(std::memory_order_acquire);
    if (old_head == NULL) { return; }
    int popper = old_head->popper_.load();
    int pusher = old_head->pusher_.load();
    if (popper != -1) { // pop is strong!
      node* next = old_head->next_.load();
      int tid = popper;
      if (old_head == head_.load(std::memory_order_acquire)) {
        if (states_[tid].is_pending()) {
          states_[tid].make_unpend();
        }
        states_[tid].node_ = old_head;
        head_.compare_exchange_strong(old_head, next);
      }
    } else if (pusher != -1){ // finish push
      node* old_next = old_head->next_.load(std::memory_order_acquire);
      int tid = pusher;
      node* old_new_next = states_[tid].node_->next_.load();
      if (old_head != head_.load(std::memory_order_acquire)) { return; };
      if (old_head != head_.load()) {
        return;
      }
      uint64_t old_phase = states_[tid].phase_.load();
      if (states_[tid].is_pending()) {
        states_[tid].make_unpend();
      }
      states_[tid].node_->next_.compare_exchange_strong(old_new_next, old_head);
      head_.compare_exchange_strong(old_head, states_[tid].node_);
      old_head->pusher_.compare_exchange_strong(tid, -1);
    }
  }

  size_t size() const {
    size_t result = 0;
    node* ptr = head_.load();
    while(ptr != NULL) {
      result++;
      ptr = ptr->next_.load();
    }
    return result;
  }

  friend std::ostream& operator<<(std::ostream& os, const stack_t& s) {
    node* ptr = s.head_.load();
    std::stringstream ss;
    ss << "Stack->";
    std::vector<node*> ptrs; // for check loop
    while(ptr != NULL) {
      ss << *ptr << "->";
      ptr = ptr->next_;

      // loop check
      typename std::vector<node*>::iterator it =
        std::find(ptrs.begin(), ptrs.end(), ptr);
      if (it != ptrs.end()) {
        os << my_index << ":" << __func__ <<
          "buffer looping!:" << ss.str() << "(LOOP) where " << *ptr << std::endl;
        return os;
      }
      ptrs.push_back(ptr);
    }
    os << ss.str() << "(NULL)";
    return os;
  }


  ~Stack() {
    node* ptr = head_.load();
    while(ptr != NULL) {
      node* next = ptr->next_;
      delete ptr;
      ptr = next;
    }
    delete[] states_.load();
  }
private:
  uint64_t max_phase() const {
    uint64_t max = 0;
    for (int i = 0; i < max_threads_; ++i) {
      if (!states_[i].is_pending()) { continue; }
      max = std::max(max, states_[i].phase_.load());
    }
    return max;
  }
  void dump_status() const {
    for (int i = 0; i < max_threads_; ++i) {
      std::cout << states_[i] << std::endl;
    }
  }
  int scan(uint64_t less) {
    for (int i = 0; i <= max_threads_; ++i) {
      if (is_still_pending(i, less)) {
        //output_ << my_index << ":" << __func__ << ":scaned tid:" << i << " is phase " << states_[i].phase_ << std::endl;
        return i;
      }
    }
    //output_ << my_index << ":" << __func__ << ": nothing to help" << std::endl;
    return -1;
  }
  std::atomic<node*> head_;
  std::atomic<state*> states_;
  int max_threads_;
  int threads_;
  std::ostream& output_;
  detail::memory_pool<node> node_pool_;
};
}// namespace nanahan
