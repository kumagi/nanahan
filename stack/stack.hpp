/* -*- Mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; compile-command: "make -j2 test" -*- */
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <pthread.h>
#include <atomic>
#include <stack>
#include <stdexcept>
#include <assert.h>

#include "../memory/qsbr.hpp"

//#include <relacy/relacy_std.hpp>

#define CACHE_LINE 64
namespace nanahan {
using namespace std;

namespace detail {
/*
template <typename T>
class concurrent_stack {
  typedef concurrent_stack<T> concurrent_stack_t;
  struct node {
    T item_;
    node* next_;
    node(const T& item):item_(item) {}
  };
  struct head_ptr {
    node* ptr_;
    uint64_t cnt_;
    head_ptr(node* ptr, uint64_t cnt):ptr_(ptr), cnt_(cnt) {}
  };
public:
  concurrent_stack():lock_(PTHREAD_MUTEX_INITIALIZER){}
  void delete_all() {
    pthread_mutex_lock(&lock_);
    while(!stack_.empty()){
      stack_.pop();
    }
    pthread_mutex_unlock(&lock_);
  }
  void push(const T& item) {
    pthread_mutex_lock(&lock_);
    stack_.push(item);
    pthread_mutex_unlock(&lock_);
  }
  T pop() {
    pthread_mutex_lock(&lock_);
    if (stack_.empty()){
      pthread_mutex_unlock(&lock_);
      throw logic_error("stack empty");
    }
    T data = stack_.top();
    stack_.pop();
    pthread_mutex_unlock(&lock_);
    return data;
  }
  stack<T> stack_;
  pthread_mutex_t lock_;
};

template <typename T>
class memory_pool {
public:
  memory_pool(){};
  template <typename t1>
  T* alloc(const t1& a1) {
    try {
      return inner_pool_.pop();
    }
    catch (const logic_error& e){
      return new T(a1);
    }
  }

  T* alloc() {
    try {
      return inner_pool_.pop();
    }
    catch (const logic_error& e){
      //cout << "new T();" << endl;
      return new T();
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
      } catch (const logic_error& e) {
        break; // do until empty
      }
    }
  }
private:
  concurrent_stack<T*> inner_pool_;
};
*/

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
//#define mb() asm volatile("":::"memory");


__thread int my_index = 0;

/*
  concurrent wait-free stack
*/
template <typename T>
class Stack {
  typedef Stack<T> stack_t;
  struct node {
    T item_;
    atomic<node*> next_;
    atomic<int> winner_;
    node(const T& item)
      :item_(item),next_(NULL), winner_(0)
    {}
    friend ostream& operator<<(ostream& os, const node& n) {
      os << "[item:" << n.item_
        << " next_:" << n.next_.load()
        << " winner:" << n.winner_.load() << "]";
      return os;
    }
  };

  struct state {
    uint64_t phase_;
    bool pending_;
    bool push_;
    node* node_;
    friend ostream& operator<<(ostream& os, const state& s) {
      os << "[phase:" << s.phase_
        << " pending:" << s.pending_
        << " push:" << ((s.push_) ? "push" : "pop")
        << " node:" << s.node_ << "]";
      return os;
    }
    state(uint64_t phase, bool pending, bool push, node* node)
      :phase_(phase), pending_(pending), push_(push), node_(node) {}

    state():phase_(0),pending_(false),push_(true),node_(NULL){}
  } __attribute__((aligned(CACHE_LINE)));
public:
  Stack(int m, ostream& os):head_(NULL), max_threads_(m),threads_(1), output_(os) {
    atomic<state*>* states = new atomic<state*>[max_threads_ + 1];
    for (int i = 0; i <= max_threads_; ++i) {
      states[i].store(NULL);
    }
    asm volatile("" : : : "memory");
    states_ = states;
  }
  void prepare() {
    while(my_index == 0) {
      my_index = __sync_fetch_and_add(&threads_, 1);
    }
  }

  void push(const T& item) {
    assert(my_index != 0);
    qsbr::ref_guard g(qsbr_);
    const uint64_t my_phase = max_phase() + 1;
    node* const new_node = new node(item);
    assert(new_node);
    new_node->winner_ = 0;
    state* const new_state = new state(my_phase, true, true, new_node);
    assert(new_state);

    if (states_[my_index].load()){
      qsbr_.safe_free(states_[my_index].load());
    }
    states_[my_index].store(new_state, memory_order_release);

    help(my_phase);
    finish();

    node* const next = new_node->next_;
    if (next != NULL) {
      next->winner_.compare_exchange_strong(my_index, 0);
    }
    //cout << my_index << ":pushed:" << item << endl;
    asm volatile("" : : : "memory");
    //assert(!states_[my_index].load(std::memory_order_seq_cst)->pending_);
    assert(!is_still_pending(my_index, my_phase));
  }

  void help_push(int tid, uint64_t phase) {
    do {
      node* old_head = head_.load();
      state* help = states_[tid].load();
      node* const pushing = help->node_;
      if (old_head != head_.load()) {
        continue;
      }
      if (old_head == NULL) {
        const uint64_t old_phase = help->phase_;
        if (is_still_pending(tid, phase)) {
          if (head_.compare_exchange_strong(old_head, pushing)) {
            help->pending_ = false;
            break;
          }
        }
      } else {
        int old_winner = old_head->winner_.load();
        if (old_head != head_.load()) { continue; }
        if (old_winner != 0) { // some thread moving
          finish();
        } else {
          if (is_still_pending(tid, phase)) {
            if (old_head->winner_.compare_exchange_strong(old_winner, tid)) {
              finish();
            }
          }
        }
      }
    } while (is_still_pending(tid, phase));
  }

  T pop() {
    assert(my_index != 0);
    qsbr::ref_guard g(qsbr_);
    const uint64_t my_phase = max_phase() + 1;
    state* const new_state = new state();
    new_state->phase_ = my_phase;
    new_state->push_ = false;
    new_state->pending_ = true;
    new_state->node_ = NULL;

    if (states_[my_index].load() != NULL) {
      qsbr_.safe_free(states_[my_index].load());
    }
    states_[my_index] = new_state;
    help(my_phase);
    finish();
    assert(!(states_[my_index].load())->pending_);
    state* const finish = states_[my_index].load(memory_order_acquire);
    if (finish->node_ == NULL) {
      cerr << "error: stack empty" << endl;
      exit(1);
      throw length_error("stack is empty");
    }
    T result(finish->node_->item_);
    qsbr_.safe_free(finish->node_);
    //cout << my_index <<":poped:" << result << endl;
    assert(!is_still_pending(my_index, my_phase));
    return result;
  }

  void help_pop(int tid, uint64_t phase){
    do {
      node* old_head = head_.load();
      state* old_state = states_[tid].load();
      if (old_head == NULL) { // linelize point
        if (is_still_pending(tid, phase)) {
          state* const finished_state = new state();
          finished_state->phase_ = old_state->phase_;
          finished_state->node_ = NULL;
          finished_state->push_ = false;
          finished_state->pending_ = false;
          if (states_[tid].compare_exchange_strong(old_state, finished_state)) {
            qsbr_.safe_free(old_state);
          } else {
            delete finished_state;
          }
          return;
        }
      }
      node* old_next = old_head->next_.load();
      int old_winner = old_head->winner_.load();
      if (old_head != head_.load()) { continue; }
      else {
        if (old_winner != 0) {
          std::cout << my_index <<": helping -> " << old_winner << std::endl;
          finish();
        }
        if (is_still_pending(tid, phase)) {
          if (head_.load()->winner_.compare_exchange_strong(old_winner, tid)) {
            finish();
          }
        }
      }
    } while(is_still_pending(tid, phase));
  }

  bool is_still_pending(int tid, uint64_t phase) {
    const state* const target = states_[tid].load();
    assert(target);
    return target->pending_ && (target->phase_ <= phase);
  }

  void help(uint64_t phase) {
    int tid;
    while(tid = scan(phase)) {
      if (states_[tid].load()->push_) {
        help_push(tid, phase);
      } else { // pop
        help_pop(tid, phase);
      }
    }
  }

  void finish() {
    // get winner thread and ensure finish it
    node* old_head = head_.load(memory_order_acquire);
    if (old_head == NULL) { return; }
    int tid = old_head->winner_.load();
    if (tid == 0) { return; }
    state* old_state = states_[tid].load(memory_order_acquire);
    if (old_state->push_) { // push
      node* const next = old_state->node_;
      if (old_head != head_.load(memory_order_acquire)) return;
      if (old_state->pending_) {
        state* const finished_state = new state();
        finished_state->phase_ = old_state->phase_;
        finished_state->pending_ = false;
        finished_state->push_ = true;
        finished_state->node_ = next;
        if (states_[tid].compare_exchange_strong(old_state, finished_state)) {
          qsbr_.safe_free(old_state);
        } else {
          delete finished_state;
        }
      }
      node* expected_next = NULL;
      next->next_.compare_exchange_strong(expected_next, old_head);
      head_.compare_exchange_strong(old_head, next);
    } else { // pop
      node* old_next = old_head->next_.load(memory_order_acquire);
      if (old_head != head_.load(memory_order_acquire)) { return; };
      if (old_state->pending_) {
        state* const finished_state = new state();
        finished_state->phase_ = old_state->phase_;
        finished_state->pending_ = false;
        finished_state->push_ = false;
        finished_state->node_ = old_head;
        if (states_[tid].compare_exchange_strong(old_state, finished_state)) {
          qsbr_.safe_free(old_state);
        } else {
          delete finished_state;
        }
      }
      head_.compare_exchange_strong(old_head, old_next);
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

  friend ostream& operator<<(ostream& os, const stack_t& s) {
    node* ptr = s.head_.load();
    stringstream ss;
    ss << "Stack->";
    vector<node*> ptrs; // for check loop
    while(ptr != NULL) {
      ss << *ptr << "->";
      ptr = ptr->next_;

      // loop check
      typename vector<node*>::iterator it =
        find(ptrs.begin(), ptrs.end(), ptr);
      if (it != ptrs.end()) {
        os << my_index << ":" << __func__ <<
          "buffer looping!:" << ss.str() << "(LOOP) where " << *ptr << endl;
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
      qsbr_.safe_free(ptr);
      ptr = next;
    }
    for (int i = 0; i < max_threads_; ++i) {
      if (states_[i].load() != NULL) {
        qsbr_.safe_free(states_[i].load());
      }
    }
  }
private:
  uint64_t max_phase() const {
    uint64_t tmp_max_phase = 0;
    for (int i = 1; i <= max_threads_; ++i) {
      const state* const target = states_[i].load(memory_order_acquire);
      if (target != NULL) {
        tmp_max_phase = max(tmp_max_phase, target->phase_);
      }
    }
    return tmp_max_phase;
  }
  void dump_status() const {
    cout << "dump_status();";
    for (int i = 0; i < max_threads_; ++i) {
      cout << states_[i] << endl;
    }
  }
  int scan(uint64_t less) {
    for (int i = 1; i <= max_threads_; ++i) {
      if (states_[i] && is_still_pending(i, less)) {
        //cout << my_index << ":" << __func__ << ":scaned tid:" << i << " is phase " << states_[i].load()->phase_ << endl;
        return i;
      }
    }
    //output_ << my_index << ":" << __func__ << ": nothing to help" << endl;
    return 0;
  }
  atomic<node*> head_;
  atomic<state*>* states_;
  int max_threads_;
  int threads_;
  ostream& output_;
  qsbr qsbr_;
};
}// namespace nanahan
