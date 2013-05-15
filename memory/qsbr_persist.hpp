// debug
#include <iostream>

#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include <new>
#include <set>

#include <boost/atomic.hpp>
//#define mb() asm volatile("" : : : "memory")
#define mb() __asm__ __volatile__ ("mfence":::"memory");


namespace detail {
const uint32_t QSBR_CACHE_LINE = 64u;
}

class qsbr {
  struct clock_chain {
    boost::atomic<uint64_t> clock_;
    clock_chain* next_;
    char padding_[detail::QSBR_CACHE_LINE - sizeof(uint64_t) - sizeof(clock_chain*)];

    clock_chain(uint64_t clock, clock_chain* next)
      :clock_(clock), next_(next) {}
    void store_seq_cst(uint64_t c) {
      clock_.store(c, boost::memory_order_seq_cst);
    }
  } __attribute__((aligned(64)));

  class base_ptr {
  public:
    virtual ~base_ptr() throw() {}
    virtual void release() = 0;
  };

  template <typename T>
  class scoped_ptr : public base_ptr {
  public:
    scoped_ptr(T* p) : ptr_(p) {}
    void release() {
      delete ptr_;
    }
  private:
    T* ptr_;
  };

  struct delete_pending_chain {
    const uint64_t clock_;
    base_ptr* const ptr_;
    delete_pending_chain* next_;
    template<typename T>
    delete_pending_chain(uint64_t clk, T* ptr)
      :clock_(clk), ptr_(new scoped_ptr<T>(ptr)), next_(NULL) { }
    ~delete_pending_chain() {
      ptr_->release();
      delete ptr_;
    }
  };

public:
  qsbr()
    :head_(NULL),
     pending_head_(NULL),
     center_clock_(1),
     chain_length_(0) {
    pthread_key_create(&key_, NULL);//operator delete);
    pthread_mutex_init(&deleting_lock_, NULL);
  }

  void set_active() {
    clock_chain* local_clock_node =
      reinterpret_cast<clock_chain*>(pthread_getspecific(key_));
    if (!local_clock_node) {
      local_clock_node = new_chain();
    }
    local_clock_node->store_seq_cst(center_clock_.load());
  }
  void set_quiescence() {
    clock_chain* local_clock_node =
      reinterpret_cast<clock_chain*>(pthread_getspecific(key_));
    if (!local_clock_node) {
      local_clock_node = new_chain();
    }
    local_clock_node->store_seq_cst(0);
  }

  void attemt_delete() {
    const uint64_t got_length = chain_length_.load();
    if (32 < got_length) {
      const int lock_result = pthread_mutex_trylock(&deleting_lock_);
      if (lock_result == 0) {
        mb();
        const uint64_t double_checked_length = chain_length_.load();
        if (32 < double_checked_length) {
          const uint64_t least_clock = scan_least_clock();
          delete_pending_chain* ptr =
            pending_head_.load(boost::memory_order_seq_cst)->next_;
          delete_pending_chain** prev_next = &ptr->next_;
          ptr = ptr->next_;
          uint64_t delete_counter = 0;
          while (ptr != NULL) {
            delete_pending_chain* const old_next = ptr->next_;
            if (ptr->clock_ < least_clock) {
              ++delete_counter;
              delete ptr;
              *prev_next = old_next;
            } else {
              prev_next = &ptr->next_;
            }
            ptr = old_next;
          }
          *prev_next = NULL;

          chain_length_.fetch_sub(delete_counter);
        }
        pthread_mutex_unlock(&deleting_lock_);
      } else if (lock_result == EBUSY) {
        //std::cout << "." << std::flush;
      } else if (lock_result == EINVAL) {
        std::cout << "uninitialized lock" << std::endl << std::flush;
      } else if (lock_result == EFAULT) {
        std::cout << "fault lock identifier" << std::endl << std::flush;
      }
    }
  }

  struct safe_delete_stack{
    void* ptr_;
    safe_delete_stack* next_;
  };
  template <typename T>
  void safe_free(T* const recipient) {
    const uint64_t delete_timing =
      center_clock_.fetch_add(1) + 1;  // increase clock
    delete_pending_chain* new_node
      = new delete_pending_chain(delete_timing, recipient);
    mb();
    for (;;) {  // cas loop for pushing chain
      delete_pending_chain* old_head = pending_head_.load();
      new_node->next_ = old_head;
      if (pending_head_.compare_exchange_strong(old_head, new_node)) {
        break;
      }
    }
    chain_length_.fetch_add(1);  // increase length
  }

  ~qsbr() {
    //std::cout << "qsbr destructer" << std::endl  << std::flush;;
    {
      pthread_mutex_lock(&deleting_lock_);
      delete_pending_chain* ptr = pending_head_.load();
      while (ptr != NULL) {
        delete_pending_chain* const old_next = ptr->next_;
        delete ptr;
        // std::cout << "~qsbr(): deleted :" << ptr << std::endl << std::flush;
        ptr = old_next;
      }
      pthread_mutex_unlock(&deleting_lock_);
      pthread_mutex_destroy(&deleting_lock_);
    }

    {
      clock_chain* ptr = head_.load();
      while (ptr) {
        clock_chain* next = ptr->next_;
        delete ptr;
        ptr = next;
      }
    }
    pthread_key_delete(key_);
  }

  class ref_guard {
  public:
    inline
    ref_guard(qsbr& target)
        :target_(&target)  {
      target_->set_active();
    }
    inline
    ~ref_guard() {
      target_->set_quiescence();
      target_->attemt_delete();
    }
  private:
    qsbr* target_;
  };

private:
  uint64_t scan_least_clock() const {
    uint64_t least_clock = ~0;  // MAX
    clock_chain* ptr = head_.load();
    while (ptr != NULL) {
      const uint64_t got_clock = ptr->clock_;
      if (0 < got_clock && least_clock < got_clock) {
        least_clock = got_clock;
      }
      ptr = ptr->next_;
    }
    return least_clock;
  }
  clock_chain* new_chain() {
    clock_chain* new_clock = new clock_chain(0, NULL);
    pthread_setspecific(key_, new_clock);
    while(true) {
      clock_chain* old_head = head_.load();
      new_clock->next_ = old_head;
      if (head_.compare_exchange_strong(old_head, new_clock)) {
        break;
      }
    }
    return new_clock;
  }

  void dump() const {
    if(0){
      const clock_chain* ptr = head_.load();
      std::cout << "clock_chain[head: "<< ptr << "] -> ";
      while (ptr) {
        std::cout << "[" << ptr->clock_ << "|" << ptr->next_ << "] -> " << std::flush;
        ptr = ptr->next_;
      }
      std::cout << "(NULL)" << std::endl << std::flush;
    }
    {
      const delete_pending_chain* ptr = pending_head_.load();
      std::cout << "pending_chain[head: "<< ptr << "] -> ";
      while (ptr) {
        std::cout << "[" << ptr->clock_ << "|" << ptr->next_ << "] -> " << std::flush;;
        ptr = ptr->next_;
      }
      std::cout << "(NULL)" << std::endl << std::flush;
    }
  }

  boost::atomic<clock_chain*> head_;
  boost::atomic<delete_pending_chain*> pending_head_;
  boost::atomic<uint64_t> center_clock_;

  pthread_mutex_t deleting_lock_;
  boost::atomic<uint64_t> chain_length_;
  pthread_key_t key_;
};
