#ifndef MAP_HPP
#define MAP_HPP
//#include <cstdint>
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include <map>
#include <functional>
#include <boost/functional/hash.hpp>
#include <memory>
#include <utility>
#include <assert.h>
#include <iterator>
#include <iostream>

namespace nanahan{
using boost::hash;
//using std::hash;

namespace detail{
template <typename T, size_t N>
struct bitcount_impl;
template <typename T>
struct bitcount_impl<T, 4> {
  static size_t call(T bits) {
    bits = (bits & 0x55555555) + (bits >> 1 & 0x55555555);
    bits = (bits & 0x33333333) + (bits >> 2 & 0x33333333);
    bits = (bits & 0x0f0f0f0f) + (bits >> 4 & 0x0f0f0f0f);
    bits = (bits & 0x00ff00ff) + (bits >> 8 & 0x00ff00ff);
    return (bits & 0x0000ffff) + (bits >>16);
  }
};
template <typename T>
struct bitcount_impl<T, 8> {
  static size_t call(T bits) {
    bits = (bits & 0x5555555555555555) + (bits >> 1 & 0x5555555555555555);
    bits = (bits & 0x3333333333333333) + (bits >> 2 & 0x3333333333333333);
    bits = (bits & 0x0f0f0f0f0f0f0f0f) + (bits >> 4 & 0x0f0f0f0f0f0f0f0f);
    bits = (bits & 0x00ff00ff00ff00ff) + (bits >> 8 & 0x00ff00ff00ff00ff);
    bits = (bits & 0x0000ffff0000ffff) + (bits >>16 & 0x0000ffff0000ffff);
    return (bits & 0x00000000ffffffff) + (bits >>32);
  }
};
template <typename T>
inline size_t bitcount(T bits) { return bitcount_impl<T, sizeof(T)>::call(bits); }

template <typename T, size_t N>
struct ntz_impl;
template <typename T>
struct ntz_impl<T, 8> {
  static size_t call(T n){
    if (n == 0) return 64;
    size_t z;
    __asm__ volatile ("bsf %1, %0;" :"=r"(z) :"r"(n));
    return z;
  }
};
template <typename T>
struct ntz_impl<T, 4> {
  static size_t call(T n){
    if (n == 0) return 32;
    size_t z;
    __asm__ volatile ("bsf %1, %0;" :"=r"(z) :"r"(n));
    return z;
  }
};

template <typename T>
inline size_t ntz(T bits) {
  return ntz_impl<T, sizeof(T)>::call(bits);}
} // detail

typedef size_t slot_size;
static const size_t one = 1;

template<typename Key,
         typename Value,
         typename Hash = hash<Key>,
         typename Pred = std::equal_to<Key>,
         typename Alloc = std::allocator<std::pair<Key, Value> >
         >
class Map{
private:
  typedef Map<Key,Value,Hash,Pred,Alloc> ThisMap;
  typedef slot_size Slot;
  static const size_t INITIAL_SIZE = 8;
  static const size_t SLOTSIZE = sizeof(Slot) * 4 - 1;
  static const size_t HOP_RANGE = SLOTSIZE * 8;
  typedef typename std::pair<Key, Value> Kvp;
  struct bucket{
#define kvp_ (*reinterpret_cast<Kvp*>(storage_))
#define ckvp_ (*reinterpret_cast<const Kvp*>(storage_))
    bucket()
      :slot_(0) {}
    void dump()const{
      std::cout << "slot[" << get_slot() << "] ";
      if (is_empty()) {
        std::cout << "<empty>";
      }else{
        std::cout << "(" << ckvp_.first << "=>" << ckvp_.second << ")";
      }
    }
    bool is_empty()const{ return (slot_ & 1) == 0;}
    Kvp& get_kvp(){ return kvp_;}
    const Kvp& get_kvp()const{ return ckvp_;}
    void set_kvp(const Kvp& kvp){
      new(&kvp_) Kvp(kvp);
      slot_ |= 1;
    }
    void delete_kvp(){
      kvp_.~Kvp();
      slot_ &= ~1;
    }
    void swap(bucket& target){
      if(target.slot_ & slot_ & 1){
        std::swap(kvp_, *reinterpret_cast<Kvp*>(target.storage_));
        bool target_is_not_empty = target.slot_ & 1;
        target.slot_ = (target.slot_ & ~1) | (slot_ & 1);
        slot_ = (slot_ & ~1) | target_is_not_empty;
      }else if(target.slot_ & 1){
        target.slot_ &= ~1;
        slot_ |= 1;
        std::memcpy(storage_, target.storage_, sizeof(Kvp));
      }else{
        slot_ &= ~1;
        target.slot_ |= 1;
        std::memcpy(target.storage_, storage_, sizeof(Kvp));
      }
    }

    void set_slot_bit(int pos) { slot_ |= one << (pos+1); }
    void drop_slot_bit(int pos) { slot_ &= ~(one << (pos+1)); }
    void clear_slot() { slot_ = 0; }
    Slot get_slot() const { return slot_ >> 1; }
    void set_slot_explicit(Slot slot) { slot_ = slot << 1 | (slot_ & 1); }
#undef kvp_
  private:
    Slot slot_; // slot_ & 1 means this kvp_ is used,
    char storage_[sizeof(Kvp)] __attribute__((aligned(sizeof(Kvp))));
  };
  static typename Alloc::template rebind<bucket>::other bucket_alloc;
  static Alloc alloc;

private:
  template <typename Derived, typename ValueType>
  class iterator_base {
  protected:
    iterator_base(bucket* b, bucket const* buckets, size_t size)
      : it_(b), buckets_(buckets), size_(size)
    {}

    ValueType* operator->() const {
      return &it_->get_kvp();
    }

    friend bool operator==(const Derived& lhs, const Derived& rhs) {
      const iterator_base& l = (const iterator_base&)lhs;
      const iterator_base& r = (const iterator_base&)rhs;
      return l.it_ == r.it_;
    }
    friend bool operator!=(const Derived& lhs, const Derived& rhs) {
      return !(lhs == rhs);
    }

    ValueType& operator*() const {
      return it_->get_kvp();
    }

    bool is_end() const { return buckets_ + size_ == it_; }

    Derived& operator++() {
      do {
        ++it_;
      } while (it_ != &buckets_[size_] && it_->is_empty());
      return static_cast<Derived&>(*this);
    }
    Derived operator++(int) {
      Derived ret = static_cast<Derived&>(*this);
      ++*this;
      return ret;
    }

    Derived& operator--() {
      do {
        --it_;
      } while (it_ != &buckets_[0] && it_->is_empty());
      return static_cast<Derived&>(*this);
    }
    Derived operator--(int) {
      Derived ret = static_cast<Derived&>(*this);
      --*this;
      return ret;
    }

    void dump() const {
      it_->dump();
    }

    void remove_unsafe(){
      it_->kvp_ = NULL;
    }

    bucket* it_;
    const bucket* buckets_;
    size_t size_;
  };

public:
  class iterator : iterator_base<iterator, std::pair<Key, Value> > {
    typedef iterator_base<iterator, std::pair<Key, Value> > base_t;

  public:
    typedef std::pair<Key, Value> value_type;
    typedef std::ptrdiff_t difference_type;
    typedef value_type* pointer;
    typedef value_type& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    iterator() : base_t(0, 0, 0) {}

    using base_t::operator->;
    using base_t::operator*;
    using base_t::is_end;
    using base_t::operator++;
    using base_t::operator--;
    using base_t::dump;
    using base_t::remove_unsafe;

  private:
    iterator(bucket* b, bucket const* buckets, size_t size)
      : base_t(b, buckets, size)
    {}

    friend class Map;
  };

  class const_iterator : public iterator_base<const_iterator, const std::pair<Key, Value> > {
    typedef iterator_base<const_iterator, const std::pair<Key, Value> > base_t;

  public:
    typedef std::pair<Key, Value> value_type;
    typedef std::ptrdiff_t difference_type;
    typedef value_type* pointer;
    typedef value_type& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    const_iterator() : base_t(0, 0, 0) {}

    const_iterator(const iterator& it)
      : base_t((const base_t&)it)
    {}

    using base_t::operator->;
    using base_t::operator*;
    using base_t::is_end;
    using base_t::operator++;
    using base_t::operator--;
    using base_t::dump;

  private:
    const_iterator(bucket* b, bucket const* buckets, size_t size)
      : base_t(b, buckets, size)
    {}

    friend class Map;
  };

private:
  static iterator const_iterator_to_iterator(const const_iterator& it) {
    return iterator(it.it_, it.buckets_, it.size_);
  }

public:
  Map(size_t initial_size = 8)
    :bucket_size_(initial_size),
     //buckets_(new bucket[bucket_size_]),
     buckets_(reinterpret_cast<bucket*>(malloc(sizeof(bucket)*bucket_size_))),
     used_size_(0),
     dont_destroy_elements_(false)
  {
    for(size_t i = 0; i < bucket_size_; ++i){ buckets_[i].clear_slot(); }
  }
  /*
  Map(const ThisMap& orig)
    :bucket_size_(orig.bucket_size_),
     buckets_(reinterpret_cast<bucket*>(malloc(sizeof(bucket)*bucket_size_))),
     used_size_(0),
     dont_destroy_elements_(false)
  {
    try {
      const_iterator it = orig.begin();
      for(; it != orig.end(); ++it){
        insert(*it);
      }
    } catch (...) {
      destroy_elements();
      free(buckets_);
      // must throw
    }
  }
  */
  Map& operator=(const Map& orig){
    Map cp(orig);
    this->swap(cp);
    return *this;
  }
  void swap(Map &other){
    std::swap(allocator_, other.allocator_);
    std::swap(bucket_size_, other.bucket_size_);
    std::swap(buckets_, other.buckets_);
    std::swap(used_size_, other.used_size_);
    std::swap(dont_destroy_elements_, other.dont_destroy_elements_);
  }
  template<typename T>
  bool operator==(const T& rhs)const{
    {
      const_iterator it = begin();
      for(; it != end(); ++it){
        typename T::const_iterator result = rhs.find(it->first);
        if (result == rhs.end()){
          return false;
        }
        if (result->second != it->second){
          return false;
        }
      }
    }
    {
      typename T::const_iterator it = rhs.begin();
      for(; it != rhs.end(); ++it){
        const_iterator result = find(it->first);
        if (result == rhs.end()){
          return false;
        }
        if (result->second != it->second){
          return false;
        }
      }
    }
    return true;
  }
  template <typename T>
  bool operator!=(const T& rhs)const{ return !this->operator==(rhs);}
  /* insert with std::pair */
  inline std::pair<iterator, bool> insert(const std::pair<Key, Value>& kvp)
  {
    const size_t hashvalue = Hash()(kvp.first);
    const size_t target_slot = locate(hashvalue, 0);
    //std::cout << "target slot:" << target_slot << std::endl;
    bucket* target_bucket = buckets_ + target_slot;
    iterator searched = find(kvp.first, hashvalue);
    if (!searched.is_end()){
      return std::make_pair(const_iterator_to_iterator(searched), false);
    }

    /* search empty bucket until max_distance*/
    bucket* empty_bucket = target_bucket;
    size_t distance = 0;
    size_t index = target_slot;
    const size_t bucket_size_minus_one = bucket_size_ - 1;
    const size_t max_distance = (HOP_RANGE < bucket_size_ ?
                                 HOP_RANGE : bucket_size_);
    do{
      if (empty_bucket->is_empty()) break;
      index = (index + 1) & bucket_size_minus_one;
      empty_bucket = &buckets_[index];
      ++distance;
    }while (distance < max_distance);

    if (max_distance <= distance){
      bucket_extend();
      return insert(kvp);
    }

    /* move empty bucket if it was too far */
    while (empty_bucket != NULL && SLOTSIZE <= distance){
      /*
      std::cout << "find closer bucket:" << 
        " " << distance << " => ";
      //*/
      swap_to_closer_bucket(&empty_bucket, &distance);
      //std::cout << distance << std::endl;
    }

    if(empty_bucket == NULL){
      std::cout << "not...found...,," << std::endl;
      dump();
      std::cout << "key:"<< kvp.first << " value:" << kvp.second
                << " target_slot:" << target_slot << std::endl; 
    }


    assert(empty_bucket->is_empty());

    //std::cout << "dist:" << distance << std::endl;
    empty_bucket->set_kvp(kvp);
    /*
      std::cout << "first:" << empty_bucket->get_kvp().first << std::endl
              << "second:" << empty_bucket->get_kvp().second << std::endl;
    */
    target_bucket->set_slot_bit(distance);
    ++used_size_;
    assert(used_size_ <= bucket_size_);
    //dump();
    return std::make_pair(iterator(empty_bucket, buckets_, bucket_size_),
                          true);
  }
  void bucket_extend(){
    const size_t old_size = bucket_size_;
    size_t new_size = old_size * 2;
    //std::cout << "resize from " << used_size_ << " of "
    //bin      << old_size << " -> " << new_size << std::endl;
    /*
    std::cout << "resizing dencity: " <<
      used_size_ << " / " << bucket_size_ << "\t| " <<
      static_cast<double>(used_size_) * 100 / bucket_size_ << " %" << std::endl;
    //*/
    while (true){
      Map new_map(new_size);
      new_map.dont_destroy_elements_ = true;
      bool ok = true;
      iterator end_it = end();
      for(iterator it = begin(); it != end_it; ++it){
        const bool result = new_map.insert_unsafe(&*it);
        if (!result){ // failed to insert
          //std::cout << "failed to insert_unsafe()" << std::endl;
          ok = false;
          break;
        }
      }
      if (!ok){
        new_size *= 2;
        continue;
      }else{
        /*
          std::cout << "old map" << std::endl;
          dump();
          std::cout << "to move map" << std::endl;
          new_map.dump();
        //*/

        std::swap(buckets_, new_map.buckets_);
        bucket_size_ = new_size;
        std::swap(allocator_, new_map.allocator_);

        /*
          std::cout << "new map" << std::endl;
          dump();
          std::cout << "deleting map" << std::endl;
          new_map.dump();
        */
        break;
      }
    }
  }

  /* erase the data*/
  void erase(const_iterator where)
  {
    bucket* const target = where.it_;
    bucket* start_bucket = target;
    if (where.is_end()){
      //std::cout << "erase: end() passed"<< std::endl;
      return;
    }
    if (find(where->first).is_end()) return;

    target->delete_kvp();
    for(Slot i = 1; i != 0; i <<= 1){
      if ((start_bucket->get_slot() & i) != 0){
        --used_size_;
        /* delete from slotinfo */
        start_bucket->set_slot_explicit(start_bucket->get_slot() & ~i);
        return;
      }
      --start_bucket;
      start_bucket += start_bucket < buckets_ ? bucket_size_ : 0;
    }
    //std::cout << "erased:" << std::endl;
  }

  iterator find(const Key& key) {
    return const_iterator_to_iterator(
      const_cast<const Map*>(this)->find(key));
  }
  const_iterator find(const Key& key) const
  {
    return find(key, Hash()(key));
  }
private:
  iterator find(const Key& key, const size_t hashvalue) {
    return const_iterator_to_iterator(
      const_cast<const Map*>(this)->find(key, hashvalue));
  }
  const_iterator find(const Key& key, const size_t hashvalue) const
  {
    const size_t target = locate(hashvalue, 0);
    bucket *target_bucket = buckets_ + target;
    Slot slot_info = target_bucket->get_slot();
    /*
    std::cout << "search:[" << target << "] for " << key <<std::endl;
    //*/
    Pred pred; // comperator
    while (slot_info){
      /*
      std::cout << "trying " << slot_info << " ";
      target_bucket->dump();
      std::cout << std::endl;
      //*/
      if ((slot_info & 1)){
        assert(target_bucket);
        if (!target_bucket->is_empty() && pred(target_bucket->get_kvp().first, key)){
          return iterator(target_bucket, buckets_, bucket_size_);
        }
        slot_info &= ~one;
      }
      const size_t gap = detail::ntz(slot_info);
      slot_info >>= gap;
      //std::cout << "new slot_info:" << slot_info << " gap:" << gap << std::endl;
      target_bucket = forward(target_bucket, gap);
    }
    //std::cout << "not found" << std::endl;
    return end();
  }

  inline void swap_to_closer_bucket(bucket** free_bucket, size_t* distance)
  {
    bucket* move_bucket = *free_bucket - SLOTSIZE + 1;
    if (move_bucket < buckets_){
      move_bucket += bucket_size_;
    }
    //std::cout << "distance:" << *distance << " ";
    for(size_t i = SLOTSIZE - 1; 0 < i; --i, move_bucket = next(move_bucket)){
      for(size_t j = 0; j < i; ++j){
        if (move_bucket->get_slot() & (one << j)){
          /*
          std::cout << "moving from " << j << " to " << i << " bucket ";
          bucket_index(move_bucket, j).dump();
          std::cout  << std::endl;
          //*/
          (*free_bucket)->swap(bucket_index(move_bucket,j));
          move_bucket->drop_slot_bit(j);
          move_bucket->set_slot_bit(i);
          *free_bucket = &bucket_index(move_bucket,j);
          *distance -= i - j;
          return;
        }
      }
    }
    *free_bucket = NULL;
    *distance = 0;
  }
public:
  iterator begin() {
    return const_iterator_to_iterator(cbegin());
  }
  iterator end() {
    return const_iterator_to_iterator(cend());
  }
  const_iterator begin() const {
    return cbegin();
  }
  const_iterator end() const {
    return cend();
  }
  const_iterator cbegin() const {
    bucket* head = buckets_;
    while (head->is_empty() && head != &buckets_[bucket_size_]){++head;}
    return const_iterator(head, buckets_, bucket_size_);
  }
  const_iterator cend() const {
    return const_iterator(&buckets_[bucket_size_], buckets_, bucket_size_);
  }

  size_t size()const{return used_size_;}
  bool empty()const{return used_size_ == 0;}
  void dump()const{
    for(size_t i=0; i < bucket_size_; ++i){
      std::cout << "[" << i << "] ";
      buckets_[i].dump();
      std::cout << std::endl;
    }
    std::cout << "total:" << used_size_ << std::endl;
  }
  void clear(){
    bucket* const new_bk =
      reinterpret_cast<bucket*>(malloc(sizeof(bucket) * INITIAL_SIZE));
    destroy_elements();
    buckets_ = new_bk;
    bucket_size_ = INITIAL_SIZE;
    used_size_ = 0;
    //delete[] (buckets_);
    free(buckets_);
  }
  ~Map(){
    if (!dont_destroy_elements_){
      destroy_elements();
    }
    free(buckets_);
    //delete[] buckets_;
  }
private:
  void destroy_elements() /* nothrow */
  {
    for(size_t i = 0; i < bucket_size_; ++i){
      if (!buckets_[i].is_empty()){
        buckets_[i].delete_kvp();
      }
    }
  }
  inline bool insert_unsafe(std::pair<Key, Value>* const kvp)
  {
    const size_t hashvalue = Hash()(kvp->first);
    const size_t target_slot = hashvalue & (bucket_size_ - 1);
    bucket* target_bucket = buckets_+ target_slot;
    //std::cout << "insert: " << kvp.first << std::endl;
    /* search empty bucket */
    bucket* empty_bucket = target_bucket;
    size_t distance = 0;
    size_t index = target_slot;
    const size_t max_distance = (HOP_RANGE < bucket_size_ ?
                                 HOP_RANGE : bucket_size_);
    do{
      if (empty_bucket->is_empty()) break;
      index = (index + 1) & (bucket_size_ - 1);
      empty_bucket = buckets_ + index;
      ++distance;
    }while (distance < max_distance);

    if (max_distance <= distance){
      return false;
    }

    /* move empty bucket if it was too far */
    while (empty_bucket != NULL && SLOTSIZE <= distance){
      swap_to_closer_bucket(&empty_bucket, &distance);
    }

    if (empty_bucket == NULL){
      return false;
    }

    assert(empty_bucket->is_empty());

    empty_bucket->set_kvp(*kvp);
    target_bucket->set_slot_bit(distance);
    return true;
  }
  inline size_t locate(size_t start, size_t diff)const{
    return (start + diff) & (bucket_size_ - 1);
  }
  inline bucket* next(bucket* b){
    return forward(b, 1);
  }
  inline bucket* forward(bucket* b, size_t stride)const{
    return (b + stride) < &buckets_[bucket_size_] ?
                          b + stride : b + stride - bucket_size_;
  }
  inline bucket& bucket_index(bucket* start, size_t index){
    bucket* target = start + index;
    return target < &buckets_[bucket_size_] ? *target : *(target - bucket_size_);
  }
  size_t bucket_size_;
  bucket* buckets_;
  size_t used_size_;
  Alloc allocator_;
  bool dont_destroy_elements_;
};


} // namespace nanahan

#endif
