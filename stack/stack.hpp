/* -*- Mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; compile-command: "make -j2 test" -*- */
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <pthread.h>
#include <stack>
#include <map>
#include <stdexcept>
#include <assert.h>
#include <boost/atomic.hpp>
#include "../memory/qsbr_persist.hpp"


//#include <relacy/relacy_std.hpp>
#ifndef safdaf
#define dd {                                    \
    pthread_mutex_lock(&index_lock_);           \
    pthread_t thread;                           \
    thread = pthread_self();                    \
    stringstream ss;                            \
    ss << "{" << thread << "%"                  \
      << my_index << "@"                        \
      << __LINE__ << "}" << endl;               \
    cout << ss.str() << flush;                  \
    pthread_mutex_unlock(&index_lock_);         \
  }
#else
#define dd
#endif

#define CACHE_LINE 64
namespace nanahan {
using namespace std;
using boost::atomic;
using boost::memory_order_seq_cst;

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
      :item_(item),next_(NULL), winner_(0) {
      mb();
    }
    friend ostream& operator<<(ostream& os, const node& n) {
      os << "[item:" << n.item_
        << " next_:" << n.next_.load()
        << " winner:" << n.winner_.load() << "]";
      return os;
    }
  };

  struct state {
    const uint64_t phase_;
    const bool pending_;
    const bool push_;
    node* node_;
    friend ostream& operator<<(ostream& os, const state& s) {
      std::stringstream ss;
      ss << &s << "[phase:" << s.phase_
        << " pending:" << s.pending_
        << " push:" << ((s.push_) ? "push" : "pop")
        << " node:" << s.node_ << "]";
      os << ss.str();
      return os;
    }
    state(uint64_t phase, bool pending, bool push, node* node)
      :phase_(phase), pending_(pending), push_(push), node_(node) {
      mb();
    }
  } __attribute__((aligned(CACHE_LINE)));
public:
  Stack(int m, ostream& os)
    :head_(NULL),
     max_threads_(m),
     threads_(1),
     output_(os),
     participated_(1) {
    pthread_key_create(&key_, operator delete);
    pthread_mutex_init(&index_lock_, NULL);

    atomic<state*>* states = new atomic<state*>[max_threads_ + 1];
    for (int i = 0; i <= max_threads_; ++i) {
      states[i].store(new state(0, false, false, NULL));
    }
    asm volatile("" : : : "memory");
    states_ = states;
  }

  inline
  int get_index() const {
    pthread_mutex_lock(&index_lock_);
    /*
      int result;
      static map<pthread_t, int> id_map;
      map<pthread_t, int>::const_iterator iter = id_map.find(pthread_self());
      if( iter != id_map.end() ) {
      result = iter->second;
      } else {
      result = id_map[pthread_self()] = id_map.size();
      }
    */
    int* my_index =
      reinterpret_cast<int*>(pthread_getspecific(key_));
    if (!my_index) {
      int* new_index = new int(participated_.fetch_add(1));
      pthread_t thread;
      thread = pthread_self();
      std::cout << "#" << *new_index << "<=>" << thread << "#";
      pthread_setspecific(key_, new_index);
      my_index = new_index;
    } else {
      static map<int,pthread_t> binding;
      map<int,uint64_t>::iterator it = binding.find(*my_index);
      pthread_t thread;
      thread = pthread_self();
      if (it == binding.end()) {
        binding.insert(make_pair(*my_index, thread));
      } else {
        assert(it->second == thread);
      }
    }//*/
    pthread_mutex_unlock(&index_lock_);
    return *my_index;
    //return result;
  }

  void push(const T& item) {
    assert(get_index());
    int my_index = get_index();
    dd;
    const uint64_t my_phase = max_phase() + 1;
    node* const new_node = new node(item);
    assert(new_node);
    state* const new_state = new state(my_phase, true, true, new_node);
    assert(new_state);
    {
      stringstream ss;
      ss << "ic:" << my_index << " phase:" << my_phase
        << " push:" << *new_state << std::endl;
      std::cout << ss.str() << std::flush;
    }

    state* old_state;
    qsbr::ref_guard g(qsbr_);
    for(;;) {
      old_state = states_[my_index].load();
      if (states_[my_index].compare_exchange_strong(old_state, new_state)){
        break;
      }
      assert(false && "something wrong");
    }

    dd;
    help(my_phase);
    dd;
    finish();
    dd;

    node* const next = new_node->next_;
    if (next != NULL) {
      int tmp_index = my_index;
      next->winner_.compare_exchange_strong(tmp_index, 0);
    }
    dd;

    qsbr_.safe_free(old_state);
    /*
      mb();
      {
      usleep(10);
      dd;
      state* const finished_state = states_[my_index].load();
      const uint64_t phase = states_[my_index].load(memory_order_seq_cst)->phase_;
      const bool pending = states_[my_index].load(memory_order_seq_cst)->pending_;
      const bool method_push = states_[my_index].load(memory_order_seq_cst)->push_;
      const bool is_pending = is_still_pending(my_index, my_phase);
      {
      stringstream ss;
      ss << "sc:" << my_index << " phase:" << my_phase
      << " push:" << *states_[my_index].load(memory_order_seq_cst)
      << std::endl;
      std::cout << ss.str() << std::flush;
      }
      if (phase != my_phase ||
      false != pending ||
      true  != method_push ||
      false != is_pending) {
      stringstream ss;
      ss << "push()|fail! thread[" << my_index << "] phase { my=>"
      << my_phase << ", loaded=>" << phase << "} "
        << " state:" << (pending ? "pending" : "safe")
        << " still_pending?:" << is_still_pending(my_index, my_phase)
        << " finished:" << *finished_state <<std::endl;
      std::cout << ss.str() << std::flush;
      usleep(1000);
      exit(1);
  }
  }
*/
    dd;
  }
  void help_push(int tid, uint64_t phase) {
    do {
      const int my_index = get_index();
      node* old_head = head_.load();
      state* help = states_[tid].load();
      node* const pushing = help->node_;
      if (old_head != head_.load()) {
        continue;
      }
      if (old_head == NULL) {
        const uint64_t old_phase = help->phase_;
        if (is_still_pending(tid, phase)) {
          if (help != states_[tid].load()) { continue; }
          if (head_.compare_exchange_strong(old_head, pushing)) {
            for (;;) {
              state* old_state = states_[tid].load();
              state* new_state =
                new state(old_state->phase_, false, true, old_state->node_);
              if (states_[tid].compare_exchange_strong(old_state, new_state)) {
                break;
              }
              break;
            }
            break;
          }
        }
      } else {
        int old_winner = old_head->winner_.load(memory_order_seq_cst);
        if (old_head != head_.load(memory_order_seq_cst)) { continue; }
        if (old_winner != 0) { // some thread working, help!
          //std::cout << my_index <<": on help_push(): helping -> " << old_winner << std::endl;
          finish();
          dd;
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
    int my_index = get_index();
    dd;
    assert(my_index != 0);
    qsbr::ref_guard g(qsbr_);
    const uint64_t my_phase = max_phase() + 1;
    state* const new_state = new state(my_phase, true, false, NULL);
    {
      stringstream ss;
      ss << "ic:" << my_index << " phase:" << my_phase
        << " pop:" << *new_state << std::endl;
      std::cout << ss.str() << std::flush;
    }
    mb();

    state* const old_state = states_[my_index].load();

    states_[my_index].store(new_state);
    dd;

    help(my_phase - 1);
    help(my_phase);
    finish();

    dd;
    assert(!(states_[my_index].load())->pending_);
    state* const my_state = states_[my_index].load(memory_order_seq_cst);
    if (my_state->node_ == NULL) {
      cerr << "error: stack empty" << endl;
      throw length_error("stack is empty");
    }
    T result(my_state->node_->item_);
    qsbr_.safe_free(my_state->node_);
    qsbr_.safe_free(old_state);
    //cout << my_index <<":poped:" << result << endl;
    /*
    mb();
    {
      usleep(10);
      state* const finished_state = states_[my_index].load();
      const uint64_t phase = states_[my_index].load(memory_order_seq_cst)->phase_;
      const bool pending = states_[my_index].load(memory_order_seq_cst)->pending_;
      const bool method_push = states_[my_index].load(memory_order_seq_cst)->push_;
      const bool is_pending = is_still_pending(my_index, my_phase);
      {
        stringstream ss;
        ss << "sc:" << my_index << " phase:" << my_phase
          << " pop:" << *states_[my_index].load(memory_order_seq_cst)
          << std::endl;
        std::cout << ss.str() << std::flush;
      }
      if (phase != my_phase ||
          false != pending ||
          false != method_push ||
          false != is_pending) {
        stringstream ss;
        ss << "pop()|fail! thread[" << my_index << "] phase { my=>"
          << my_phase << ", loaded=>" << phase << "} "
          << " state:" << (pending ? "pending" : "safe")
          << " still_pending?:" << is_still_pending(my_index, my_phase)
          << " finished_state:" << *finished_state << std::endl;
        std::cout << ss.str() << std::flush;
        usleep(100);
        exit(1);
      }
    }
    */
    return result;
  }
  void help_pop(int tid, uint64_t phase){
    do {
      int my_index = get_index();
      node* old_head = head_.load();
      state* old_state = states_[tid].load();
      if (old_head == NULL) { // linelize point
        if (is_still_pending(tid, phase)) {
          state* const finished_state =
            new state(old_state->phase_, false, false, NULL);
          if (states_[tid].compare_exchange_strong(old_state, finished_state)) {
            std::cout << my_index << ":" << *finished_state << " empty_pop()" <<std::endl;
            qsbr_.safe_free(old_state);
          } else {
            delete finished_state;
          }
        }
        return;
      }
      node* old_next = old_head->next_.load();
      int old_winner = old_head->winner_.load();
      if (old_head != head_.load(memory_order_seq_cst)) { continue; }
      else {
        if (old_winner > 0) {
          std::cout << my_index <<": on help_pop(): helping -> " << old_winner << std::endl;
          finish();
          continue;
        }
        if (old_winner < 0) {
          old_head->winner_.compare_exchange_strong(old_winner, 0);
        }
        if (is_still_pending(tid, phase)) {
          int tmp = old_winner;
          if (old_head->winner_.compare_exchange_strong(tmp, tid)) {
            finish();
          }
        }
      }
    } while(is_still_pending(tid, phase));
  }

  bool is_still_pending(const int tid, const uint64_t phase) const {
    const state* target = states_[tid].load(memory_order_seq_cst);
    assert(target);
    bool result;
    while (true) {
      result = target->pending_ && (target->phase_ <= phase);
      const state* new_target = states_[tid].load(memory_order_seq_cst);
      if (target != new_target) {
        target = new_target;
        continue;
      }
      break;
    }
    return result;
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

  bool must_continue(uint64_t phase) const {
    return is_still_pending(get_index(), phase);
  }

  void finish() {
    // get winner thread and ensure finish it
    for (;;) {
      node* old_head = head_.load(memory_order_seq_cst);
      if (old_head == NULL) { return; }
      int tid = old_head->winner_.load();
      int my_index = get_index();
      if (tid == 0) { return; }
      state* old_state = states_[tid].load(memory_order_seq_cst);
      const bool method_is_push = old_state->push_;
      if (old_head != head_.load(memory_order_seq_cst) ||
          tid != old_head->winner_.load() ||
          old_state != states_[tid].load(memory_order_seq_cst)) {
        continue;
      }
      dd;
      if (method_is_push) { // push
        dd;
        node* const next = old_state->node_;
        if (old_head != head_.load(memory_order_seq_cst)) { return; }
        if (old_state->pending_ && 0 < tid) {
          state* const finished_state = new state(old_state->phase_, false, true, next);
          mb();
          old_head->winner_.compare_exchange_strong(tid, minus_tid);
          if (states_[tid].compare_exchange_strong(old_state, finished_state)) {
            std::cout << my_index << " help push ";
            dump_status_index(tid);
            qsbr_.safe_free(old_state);
          } else {
            delete finished_state;
          }
          dd;
        }
        usleep(100);
        node* expected_next = NULL;
        int minus_tid = -tid;
        next->next_.compare_exchange_strong(expected_next, old_head);
        head_.compare_exchange_strong(old_head, next);
        old_head->winner_.compare_exchange_strong(minus_tid, 0);
        dd;
      } else { // pop
        dd;
        node* old_next = old_head->next_.load(memory_order_seq_cst);
        if (old_head != head_.load(memory_order_seq_cst)) { return; };
        if (old_state->pending_) {
          state* const finished_state =
            new state(old_state->phase_, false, false, old_head);
          mb();

          if (states_[tid].compare_exchange_strong(old_state, finished_state)) {
            std::cout << my_index << " help pop ";
            dump_status_index(tid);
            qsbr_.safe_free(old_state);
          } else {
            dd;
            delete finished_state;
          }
        }
        head_.compare_exchange_strong(old_head, old_next);
        dd;
      }
      break;
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
        os << s.get_index() << ":" << __func__ <<
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
    for (int i = 0; i <= max_threads_; ++i) {
      state* got_state = states_[i].load();
      if (got_state) {
        qsbr_.safe_free(got_state);
      }
    }
    delete[] states_;

  }
private:
  uint64_t load_phase(int tid) const {
    for(;;) {
      const state* const target = states_[tid].load(memory_order_seq_cst);
      uint64_t tmp_phase = target->phase_;
      if (target == states_[tid].load(memory_order_seq_cst)) {
        return tmp_phase;
      }
    }
  }
  uint64_t max_phase() const {
    uint64_t result = 0;
    for (int i = 1; i <= max_threads_; ++i) {
      result = max(result, load_phase(i));
    }
    return result;
  }
  void dump_status() const {
    cout << "dump_status();";
    for (int i = 0; i <= max_threads_; ++i) {
      cout << states_[i] << endl;
    }
  }

  void dump_status_index(int i) const {
    cout << "st(" << i << ")[" << *states_[i].load() << "]" << endl;
  }

  int scan(uint64_t less) const {
    for (int i = 1; i <= max_threads_; ++i) {
      if (states_[i] && is_still_pending(i, less)) {
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

  mutable pthread_mutex_t index_lock_;

  mutable atomic<int> participated_;
  pthread_key_t key_;
};
}// namespace nanahan
