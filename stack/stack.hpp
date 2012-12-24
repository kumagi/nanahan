/* -*- Mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; compile-command: "make -j2 test" -*- */
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <pthread.h>
#include <atomic>

//#include <relacy/relacy_std.hpp>

#define CACHE_LINE 64
namespace nanahan {

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
    std::atomic<node*> prev_;
    int pusher_;
    node(const T& item, uint64_t phase, int pusher)
      :item_(item),next_(NULL), prev_(NULL), pusher_(pusher)
    {}
    friend std::ostream& operator<<(std::ostream& os, const node& n) {
      os << "[item:" << n.item_
         << " next_:" << n.next_.load() << " prev_:" << n.prev_.load()
         << " pusher:" << n.pusher_ << "]";
      return os;
    }
  };

  struct state {
    std::atomic<uint64_t> phase_; /* even=not pending, odd=pending */
    bool push_;
    node* node_;
    friend std::ostream& operator<<(std::ostream& os, const state& s) {
      os << "[phase:" << s.phase_
         << " push:" << ((s.push_ == true) ? "push" : "pop")
         << " node_:" << s.node_ << "]";
      return os;
    }
    bool is_pending(){
      uint64_t phase = phase_.load();
      return phase & 1;
    }
    void make_pending() {
      uint64_t old_phase = phase_.load(std::memory_order_relaxed);
      phase_.store(old_phase | 1 );
    }
    void make_unpend() {
      uint64_t old_phase = phase_.load(std::memory_order_relaxed);
      if((old_phase & 1) == 0){
        return;
      }
      phase_.compare_exchange_strong(old_phase, old_phase + 1);
    }
  } __attribute__((aligned(CACHE_LINE)));
public:
  Stack(int m, std::ostream& os):head_(NULL), max_threads_(m),threads_(0), output_(os) {
    states_.store(new state[max_threads_ + 1]);
    for (int i=0; i<=max_threads_; ++i) {
      states_[i].phase_.store(0); // not pending
      states_[i].push_ = false;
      states_[i].node_ = NULL;
    }
  }
  void prepare() {
    while(my_index == 0) {
      my_index = __sync_fetch_and_add(&threads_, 1);
    }
  }
  void push(const T& item) {
    const uint64_t my_phase = (max_phase() | 1);
    node* const new_node = new node(item, my_phase, my_index);
    states_[my_index].push_ = true;
    states_[my_index].node_ = new_node;

    states_[my_index].phase_.store(my_phase);

    //output_ << my_index << ":" << __func__ <<":node set " << states_[my_index] << *new_node << std::endl;

    help(my_phase);
    //output_ << my_index << ":" << __func__ <<":helped:" << my_phase << " and finishing" << std::endl;
    finish();
  }

  bool is_still_pending(int tid, uint64_t phase) {
    uint64_t its_phase = states_[tid].phase_.load();
    return (its_phase & 1) && (its_phase <= phase);
  }

  void help(uint64_t phase) {
    int tid;
    while((tid = scan(phase)) != -1) {
      if(states_[tid].push_ == true) {
        node* old_head = head_.load(std::memory_order_relaxed);
        node* pushing = states_[tid].node_;
        node* old_next = pushing->next_.load();
        //output_ << my_index << ":" << __func__ <<": start " << tid << ":" << states_[tid] << " node " << *pushing << std::endl;
        if(old_head != head_.load()) { continue; }
        if(old_head == NULL){
          //output_ << my_index << ":" << __func__ <<": pushing for NULL stack:" << pushing;

          uint64_t old_phase = states_[tid].phase_.load();
          if(is_still_pending(tid, phase)){
            if(head_.compare_exchange_strong(old_head, pushing)) {
              states_[tid].make_unpend();
              //output_ << my_index << ":" << __func__ <<": easy push success" << std::endl;
              break;
            }
          }
          //output_ << my_index << ":" << __func__ <<": easy push failed, retry" << std::endl;
          continue;
        }else{
          node* old_prev = old_head->prev_.load();
          if(old_head != head_.load()){ continue; }
          if(old_prev != NULL){
            //output_ << my_index << ":" << __func__ <<": seems to need help, call finish()" << std::endl;
            finish();
          }else{
            //output_ << my_index << ":" << __func__ <<
            //":cas head->prev: NULL =>" << pushing << std::endl;
            if(is_still_pending(tid, phase)){
              if(old_head->prev_.compare_exchange_strong(old_prev, pushing)){
                //output_ << my_index << ":" << __func__ <<":success to reserve" << std::endl;
                finish();
                // need not break, because of scan will fail if we finish()ed properly
                //output_ << my_index << ":" << __func__ <<": finished" << std::endl;
              }else{
                //output_ << my_index << ":" << __func__ <<":cas head fail." << std::endl;
              }
            }
          }
        }
      }else{ // pop
      }
    }
  }
  void finish() {// read head
    while(true){
      node* old_head = head_.load(std::memory_order_acquire);
      if(old_head == NULL){return;}
      node* old_prev = old_head->prev_.load(std::memory_order_relaxed);
      node* old_next = old_head->next_.load(std::memory_order_relaxed);
      if(old_head != head_.load(std::memory_order_seq_cst)){ continue; };
      if(old_prev == NULL){
        //output_ << my_index << ":" << __func__ <<":nothing TODO" << std::endl;
        return;
      }
      node* prev_next = old_prev->next_;
      const int tid = old_prev->pusher_; // finishing target
      if(old_head != head_.load()) {
        //output_ << my_index << ":" << __func__ <<":old_head(): head unmatch retry" << std::endl;
        continue;
      }
      if(tid == -1) {
        //output_ << my_index << ":" << __func__ <<":old_head(): thread already dead" << std::endl;
        break;
      }
      uint64_t old_phase = states_[tid].phase_.load();
      if(states_[tid].is_pending()){
        //output_ << my_index << ":" << __func__ <<":thread " << tid << " pending, make it unpending" << std::endl;
        states_[tid].make_unpend();
        //output_ << my_index << ":" << __func__ <<":thread " << tid << " unpend, phase is " << states_[tid].phase_.load() << std::endl;
      }

      states_[tid].node_->next_.compare_exchange_strong(prev_next, old_head);
      head_.compare_exchange_strong(old_head, old_prev);
      old_head->prev_.compare_exchange_strong(old_prev,(node*)NULL);
    }
  }

  size_t size()const{
    size_t result = 0;
    node* ptr = head_.load();
    while(ptr != NULL){
      result++;
      ptr = ptr->next_.load();
    }
    return result;
  }

  T pop(){}

  friend std::ostream& operator<<(std::ostream& os, const stack_t& s){
    node* ptr = s.head_.load();
    std::stringstream ss;
    ss << "Stack->";
    std::vector<node*> ptrs; // for check loop
    while(ptr != NULL){
      ss << *ptr << "->";
      ptr = ptr->next_;

      // loop check
      typename std::vector<node*>::iterator it =
        std::find(ptrs.begin(), ptrs.end(), ptr);
      if(it != ptrs.end()){
        os << my_index << ":" << __func__ <<
          "buffer looping!:" << ss.str() << "(LOOP) where " << *ptr << std::endl;
        return os;
      }
      ptrs.push_back(ptr);
    }
    os << ss.str() << "(NULL)";
    return os;
  }


  ~Stack(){
    node* ptr = head_.load(std::memory_order_relaxed);
    while(ptr != NULL){
      node* next = ptr->next_;
      delete ptr;
      ptr = next;
    }
    delete[] states_.load();
  }
private:
  uint64_t max_phase()const{
    uint64_t max = 0;
    for(int i=0; i < max_threads_; ++i){
      max = std::max(max, states_[i].phase_.load());
    }
    return max;
  }
  void dump_status()const{
    for(int i=0; i < max_threads_; ++i){
      //output_ << "i:" << i << "[" << states_[i].phase_ << "]" << std::endl;
    }
  }
  int scan(uint64_t less){
    for(int i=0; i <= max_threads_; ++i){
      if(is_still_pending(i, less)){
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
};
}// namespace nanahan
