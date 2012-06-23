#ifndef MAP_HPP
#define MAP_HPP
//#include <cstdint>
#include <stdint.h>
#include <cstddef>
#include <map>
#include <functional>
#include <boost/functional/hash.hpp>
#include <memory>
#include <utility>
#include <assert.h>

#include <iostream>

namespace nanahan{
using boost::hash;
//using std::hash;

namespace detail{
size_t bitcount(uint32_t bits) {
  bits = (bits & 0x55555555LU) + (bits >> 1 & 0x55555555LU);
  bits = (bits & 0x33333333LU) + (bits >> 2 & 0x33333333LU);
  bits = (bits & 0x0f0f0f0fLU) + (bits >> 4 & 0x0f0f0f0fLU);
  bits = (bits & 0x00ff00ffLU) + (bits >> 8 & 0x00ff00ffLU);
  return (bits & 0x0000ffffLU) + (bits >>16 & 0x0000ffffLU);
}

size_t bitcount(uint64_t bits) {
  bits = (bits & 0x5555555555555555LLU) + (bits >> 1 & 0x5555555555555555LLU);
  bits = (bits & 0x3333333333333333LLU) + (bits >> 2 & 0x3333333333333333LLU);
  bits = (bits & 0x0f0f0f0f0f0f0f0fLLU) + (bits >> 4 & 0x0f0f0f0f0f0f0f0fLLU);
  bits = (bits & 0x00ff00ff00ff00ffLLU) + (bits >> 8 & 0x00ff00ff00ff00ffLLU);
  bits = (bits & 0x0000ffff0000ffffLLU) + (bits >>16 & 0x0000ffff0000ffffLLU);
  return (bits & 0x00000000ffffffffLLU) + (bits >>32 & 0x00000000ffffffffLLU);
}

}

//#define nanahan_64bit
#ifdef nanahan_64bit
# define slot_size uint64_t
# define one 1LLU
#else
# define slot_size uint32_t
# define one 1LU
#endif


template<typename Key,
         typename Value,
         typename Hash = hash<Key>,
         typename Pred = std::equal_to<Key>,
         typename Alloc = std::allocator<std::pair<const Key, Value> >
         >
class Map{
private:
  typedef Map<Key,Value,Hash,Pred,Alloc> ThisMap;
  typedef slot_size Slot;
  static const uint64_t INITIAL_SIZE = 8;
  static const uint32_t SLOTSIZE = sizeof(Slot) * 8;
  static const uint32_t HOP_RANGE = SLOTSIZE * 8;
  typedef typename std::pair<const Key, Value> Kvp;
  struct bucket{
    Slot slot_;
    std::pair<const Key, Value> *kvp_;
    bucket()
      :slot_(0), kvp_(NULL){}
    void dump()const{
       std::cout << "slot[" << slot_ << "] ";
      if(kvp_ == NULL) {
        std::cout << "<empty>";
      }else{
        std::cout << "(" << kvp_->first << "=>" << kvp_->second << ")";
      }
    }
  };
  static typename Alloc::template rebind<bucket>::other bucket_alloc;
  static Alloc alloc;
public:
  class iterator{
  public:
    iterator(bucket* b, bucket* buckets, size_t size)
      :it_(b), buckets_(buckets),size_(size){}
    const std::pair<const Key, Value>* operator->()const{return it_->kvp_;}
    std::pair<const Key, Value>* operator->(){return it_->kvp_;}
    //iterator(const iterator&) = default;
    //iterator(iterator&& i):it_(i.it_),buckets_(i.buckets_),size_(i.size_){}
    bool operator==(const iterator& rhs)const{ return it_ == rhs.it_; }
    bool operator!=(const iterator& rhs)const{ return !operator==(rhs); }
    const std::pair<const Key, Value>& operator*()const{ return *it_->kvp_; }
    std::pair<const Key, Value>& operator*(){ return *it_->kvp_; }
    bool is_end()const{ return buckets_ + size_ == it_; }
    iterator operator++(){
      do{
        ++it_;
      }while(it_ != &buckets_[size_] && it_->kvp_ == NULL);
      return *this;
    }
    iterator operator--(){
      do{
        --it_;
      }while(it_ != &buckets_[0] && it_->kvp_ == NULL);
      return *this;
    }
    void dump()const{
      it_->dump();
    }
  private:
    void remove_unsafe(){
      it_->kvp_ = NULL;
    }
    iterator();
    bucket* it_;
    const bucket* const buckets_;
    const size_t size_;
    friend class nanahan::Map<Key,Value,Hash,Pred,Alloc>;
  };
  Map(size_t initial_size = 8)
    :bucket_size_(initial_size),
     buckets_(new bucket[bucket_size_]),
     used_size_(0),
     extending_(false)
  {}
  Map(const ThisMap& orig)
    :bucket_size_(orig.bucket_size_),
     buckets_(new bucket[bucket_size_]),
     used_size_(0),
     extending_(false)
  {
    iterator it = orig.begin();
    for(; it != orig.end(); ++it){
      insert(*it);
    }
  }
  Map& operator=(const Map& orig){
    for(size_t i = 0; i < bucket_size_; ++i){
      if(buckets_[i].kvp_ != NULL){
        buckets_[i].kvp_->~Kvp();
        allocator_.deallocate(buckets_[i].kvp_, 1);
        buckets_[i].kvp_ = NULL;
      }
      buckets_[i].slot_ = 0;
    }
    used_size_ = 0;
    if(bucket_size_ != orig.bucket_size_){
      delete[] buckets_;
      buckets_ = new bucket[orig.bucket_size_];
    }
    iterator it = orig.begin();
    for(; it != orig.end(); ++it){
      insert(*it);
    }
    return *this;
  }
  template<typename T>
  bool operator==(const T& rhs)const{
    {
      iterator it = begin();
      for(; it != end(); ++it){
        typename T::iterator result = rhs.find(it->first);
        if(result == rhs.end()){
          return false;
        }
        if(result->second != it->second){
          return false;
        }
      }
    }
    {
      typename T::iterator it = rhs.begin();
      for(; it != rhs.end(); ++it){
        iterator result = find(it->first);
        if(result == rhs.end()){
          return false;
        }
        if(result->second != it->second){
          return false;
        }
      }
    }
    return true;
  }
  template <typename T>
  bool operator!=(const T& rhs)const{ return !this->operator==(rhs);}
  /* insert with std::pair */
  inline std::pair<iterator, bool> insert(const std::pair<const Key, Value>& kvp)
  {
    const size_t hashvalue = Hash()(kvp.first);
    const size_t target_slot = locate(hashvalue, 0);
    //std::cout << "target slot:" << target_slot << std::endl;
    bucket* target_bucket = buckets_ + target_slot;
    iterator searched = find(kvp.first, hashvalue);
    if(!searched.is_end()){
      return std::make_pair(searched, false);
    }

    //std::cout << "insert: " << kvp.first << std::endl;
    /* search empty bucket */
    bucket* empty_bucket = target_bucket;
    size_t distance = 0;
    size_t index = target_slot;
    const size_t max_distance = (HOP_RANGE < bucket_size_ ?
                                 HOP_RANGE : bucket_size_);
    do{
      if(empty_bucket->kvp_ == NULL) break;
      index = (index + 1) & (bucket_size_ - 1);
      empty_bucket = &buckets_[index];
      ++distance;
    }while(distance < max_distance);

    if(max_distance <= distance){
      bucket_extend();
      return insert(kvp);
    }

    /* move empty bucket if it was too far */
    while(empty_bucket != NULL && SLOTSIZE <= distance){
      /*
      std::cout << "find closer bucket:" << std::endl;
      std::cout << "distance : " << distance << " => ";
      //*/
      find_closer_bucket(&empty_bucket, &distance);
      //std::cout << distance << std::endl;
    }

    if(empty_bucket == NULL){
      bucket_extend();
      return insert(kvp);
    }

    assert(empty_bucket->kvp_ == NULL);

    empty_bucket->kvp_ = allocator_.allocate(1);
    allocator_.construct(empty_bucket->kvp_, kvp);
    //std::cout << "dist:" << distance << std::endl;
    target_bucket->slot_ |= one << distance;
    ++used_size_;
    //dump();
    return std::make_pair(iterator(empty_bucket, buckets_, bucket_size_),
                          true);
  }
  void bucket_extend(){
    const size_t old_size = bucket_size_;
    size_t new_size = old_size * 2;
    /*
      std::cout << "resize from " << used_size_ << " "
      << old_size << " -> " << new_size << std::endl;
    std::cout << "extending dencity: " <<
      used_size_ << " / " << bucket_size_ << "\t| " <<
      static_cast<double>(used_size_) * 100 / bucket_size_ << " %" << std::endl;
    //*/
    while(true){
      Map new_map(new_size);
      bool ok = true;
      iterator end_it = end();
      for(iterator it = begin(); it != end_it; ++it){
        const bool result = new_map.insert_unsafe(&*it);
        if(!result){ // failed to insert
          std::cout << "failed to insert_unsafe()" << std::endl;
          ok = false;
          break;
        }
      }
      if(!ok){
        new_size *= 2;
        new_map.clear_unsafe();
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
        new_map.clear_unsafe();

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
  iterator erase(iterator where)
  {
    bucket* const target = where.it_;
    bucket* start_bucket = target;
    if(where.is_end()){
      //std::cout << "erase: end() passed"<< std::endl;
      return end();
    }
    if(find(where->first).is_end()){return end();}

    //std::cout << "erase!" << slot->kvp_->first << std::endl;
    allocator_.destroy(target->kvp_);
    allocator_.deallocate(target->kvp_, 1);
    target->kvp_ = NULL;
    for(Slot i = 1; i != 0; i <<= 1){
      if((start_bucket->slot_ & i) != 0){
        --used_size_;
        start_bucket->slot_ &= ~i;
        return iterator(target , buckets_, bucket_size_);
      }
      --start_bucket;
    }
    //std::cout << "erased:" << std::endl;
    return end();
  }

  iterator find(const Key& key) const
  {
    return find(key, Hash()(key));
  }
  iterator find(const Key& key, const size_t hashvalue)const
  {
    const unsigned int target = locate(hashvalue, 0);
    bucket *target_bucket = buckets_ + target;
    Slot slot_info = target_bucket->slot_;
    /*
    std::cout << "search:[" << target << "] for " << key <<std::endl;
    //*/
    Pred pred; // comperator
    while(slot_info){
      /*
      std::cout << "trying " << slot_info << " ";
      target_bucket->dump();
      std::cout << std::endl;
      //*/
      if((slot_info & 1)){
        assert(target_bucket);
        if(pred(target_bucket->kvp_->first, key)){
          return iterator(target_bucket, buckets_, bucket_size_);
        }
        slot_info &= ~1;
      }
      const size_t gap = detail::bitcount((~slot_info) & (slot_info - 1));
      slot_info >>= gap;
      //std::cout << "new slot_info:" << slot_info << " gap:" << gap << std::endl;
      target_bucket= forward(target_bucket, gap);
    }
    //std::cout << "not found" << std::endl;
    return end();
  }

  inline void find_closer_bucket(bucket** free_bucket, size_t* distance)
  {
    bucket* move_bucket = *free_bucket - SLOTSIZE + 1;
    if(move_bucket < buckets_){
      move_bucket += bucket_size_;
    }

    for(size_t i = SLOTSIZE - 1; 0 < i; --i, move_bucket = next(move_bucket)){
      for(size_t j = 0; j <= i; ++j){
        if(move_bucket->slot_ & (one << j)){
          (*free_bucket)->kvp_ = bucket_index(move_bucket, j).kvp_;
          /*
          std::cout << "moving from " << j << " to " << i << "bucket";
          bucket_index(move_bucket, j).dump();
          std::cout  << std::endl;
          //*/
          bucket_index(move_bucket,j).kvp_ = NULL;
          move_bucket->slot_ &= ~(one << j);
          move_bucket->slot_ |= one << i;
          *free_bucket = &bucket_index(move_bucket,j);
          *distance -= i - j;
          return;
        }
      }
    }
    *free_bucket = NULL;
    *distance = 0;
  }
  iterator begin()const{
    bucket* head = buckets_;
    while(head->kvp_ == NULL && head != &buckets_[bucket_size_]){++head;}
    return iterator(head, buckets_, bucket_size_);
  }
  iterator end()const{
    return iterator(&buckets_[bucket_size_], buckets_, bucket_size_);
  }
  iterator begin(){
    bucket* head = buckets_;
    while(head->kvp_ == NULL && head != &buckets_[bucket_size_]){++head;}
    return iterator(head, buckets_, bucket_size_);
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
    for(size_t i = 0; i < bucket_size_; ++i){
      if(buckets_[i].kvp_ != NULL){
        buckets_[i].kvp_->~Kvp();
        allocator_.deallocate(buckets_[i].kvp_, 1);
      }
      buckets_[i].slot_ = 0;
    }
    delete[] buckets_;
    buckets_ = new bucket[INITIAL_SIZE];
    bucket_size_ = INITIAL_SIZE;
    used_size_ = 0;
  }
  ~Map(){
    //dump();
    if(!extending_){
      clear();
    }
    delete[] buckets_;
  }
private:
  inline void clear_unsafe(){
    extending_ = true;
    return;
  }
  inline bool insert_unsafe(std::pair<const Key, Value>* const kvp)
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
      if(empty_bucket->kvp_ == NULL) break;
      index = (index + 1) & (bucket_size_ - 1);
      empty_bucket = buckets_ + index;
      ++distance;
    }while(distance < max_distance);

    if(max_distance <= distance){
      return false;
    }

    /* move empty bucket if it was too far */
    while(empty_bucket != NULL && SLOTSIZE <= distance){
      find_closer_bucket(&empty_bucket, &distance);
    }

    if(empty_bucket == NULL){
      return false;
    }

    assert(empty_bucket->kvp_ == NULL);

    empty_bucket->kvp_ = kvp;
    target_bucket->slot_ |= one << distance;
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
  uint64_t bucket_size_;
  bucket* buckets_;
  size_t used_size_;
  Alloc allocator_;
  bool extending_;
};


} // namespace nanahan
#undef nanahan_64bit
#undef one
#undef slot_size
#endif
