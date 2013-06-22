#include "map.hpp"
#include <gtest/gtest.h>
#include <string>
#include <set>
#include <memory.h>
#include <boost/random.hpp>

extern template class nanahan::Map<int, int>;
extern template class nanahan::Map<uint64_t, std::string>;
extern template class nanahan::Map<std::string, std::string>;
extern template class nanahan::Map<bool, bool>;
extern template class nanahan::Map<std::string, int>;
extern template class nanahan::Map<std::string, uint32_t>;
TEST(map, construct){
  nanahan::Map<int, int> m;
  nanahan::Map<uint64_t, std::string> n;
  nanahan::Map<std::string, std::string > o;
  nanahan::Map<bool, bool> p;
}

TEST(map, is_empty){
  nanahan::Map<int, int> m;
  ASSERT_TRUE(m.empty());
  for (int i = 0; i<1000; ++i){
    ASSERT_EQ(m.find(i), m.end());
  }
  ASSERT_TRUE(m.sanity_check());
}


TEST(map, insert_one){
  nanahan::Map<int, int> m;
  {
    std::pair<nanahan::Map<int, int>::iterator, bool> result =
    m.insert(std::make_pair(8, 9));
    ASSERT_TRUE(m.sanity_check());
    ASSERT_TRUE(result.second);
    ASSERT_EQ(8, m.find(8)->first);
    ASSERT_EQ(9, m.find(8)->second);
  }
  {
    std::pair<nanahan::Map<int, int>::iterator, bool> result =
      m.insert(std::make_pair(8, 9));
    ASSERT_TRUE(m.sanity_check());
    ASSERT_FALSE(result.second);
  }
  ASSERT_EQ(m.find(2), m.end());
}


TEST(map, insert_one_string){
  nanahan::Map<int, std::string> m;
  {
    std::pair<nanahan::Map<int, std::string>::iterator, bool> result =
    m.insert(std::make_pair(8, "9"));
    ASSERT_TRUE(m.sanity_check());
    ASSERT_TRUE(result.second);
    ASSERT_EQ(m.find(8)->first, 8);
    ASSERT_EQ(m.find(8)->second, "9");
  }
  {
    std::pair<nanahan::Map<int, std::string>::iterator, bool> result =
      m.insert(std::make_pair(8, "10"));
    ASSERT_TRUE(m.sanity_check());
    ASSERT_FALSE(result.second);
  }
  ASSERT_EQ(m.find(2), m.end());
}

TEST(map, insert_many){
  nanahan::Map<int, int> m;
  for (int i = 1; i<8; i += 2){
    m.insert(std::make_pair(i, i*i));
    ASSERT_TRUE(m.sanity_check());
  }
  for (int i = 0; i<8; i += 2){
    ASSERT_EQ(m.find(i), m.end());
    ASSERT_NE(m.find(i+1), m.end());
    ASSERT_EQ(m.find(i+1)->first, i+1);
    ASSERT_EQ(m.find(i+1)->second, (i+1)*(i+1));
  }
}

TEST(map, insert_eight){
  nanahan::Map<int, int> m;
  for (int i = 0; i<8; ++i){
    m.insert(std::make_pair(i, i*i));
    ASSERT_TRUE(m.sanity_check());
  }
  //m.dump();
  for (int i = 0; i<8; ++i){
    ASSERT_NE(m.find(i), m.end());
    ASSERT_EQ(m.find(i)->first, i);
    ASSERT_EQ(m.find(i)->second, i*i);
  }
}

TEST(map, insert_eight_string){
  nanahan::Map<std::string, int> m;
  char orig[] = "hogedsfdsaa";
  for (int i = 0; i<8; ++i){
    orig[2] = i + '0';
    orig[4] = i + '0';
    m.insert(std::make_pair(std::string(orig), i*i));
    std::cout << "inserted " << orig << std::endl;
    ASSERT_TRUE(m.sanity_check());
    //m.dump();
  }
  //m.dump();
  for (int i = 0; i<8; ++i){
    orig[2] = i + '0';
    orig[4] = i + '0';
    nanahan::Map<std::string, int>::const_iterator it = m.find(std::string(orig));
    ASSERT_NE(it, m.end());
    ASSERT_NE(m.find(std::string(orig)), m.end());
    ASSERT_EQ(m.find(std::string(orig))->first, std::string(orig));
    ASSERT_EQ(m.find(std::string(orig))->second, i*i);
  }
}
TEST(map, bucket_extend){
  nanahan::Map<int, int> m;
  const int size = 10000;
  for (int i = 0; i < size; ++i){
    m.insert(std::make_pair(i, i*i));
    if ((i & 7) == 0) {
      ASSERT_TRUE(m.sanity_check());
    }
  }
  for (int i = 0; i < size; ++i){
    ASSERT_NE(m.find(i), m.end());
    ASSERT_EQ(m.find(i)->first, i);
    ASSERT_EQ(m.find(i)->second, i*i);
  }
}

TEST(map, erase){
  nanahan::Map<int, int> m;
  std::pair<nanahan::Map<int,int>::iterator, bool> result =
    m.insert(std::make_pair(8, 9));
  ASSERT_TRUE(m.sanity_check());
  ASSERT_NE(m.find(8), m.end());
  ASSERT_EQ(m.find(8)->first, 8);
  ASSERT_EQ(m.find(8)->second, 9);
  ASSERT_EQ(m.find(2), m.end());
  m.dump();
  m.erase(result.first);
  m.dump();
  ASSERT_TRUE(m.sanity_check());
  ASSERT_EQ(m.find(8), m.end());
}


TEST(map, erase_many){
  nanahan::Map<int, int> m;
  const int size = 1000;
  for (int i = 0; i < size; ++i){
    m.insert(std::make_pair(i, i*i));
    ASSERT_TRUE(m.sanity_check());
  }
  for (int i = 0; i < size; ++i){
    m.erase(m.find(i));
    ASSERT_TRUE(m.sanity_check());
  }
  for (int i = 0; i < size; ++i){
    ASSERT_EQ(m.find(i), m.end());
  }
}

TEST(map, erase_half){
  nanahan::Map<int, int> m;
  const int size = 10000;
  for (int i = 0; i < size; ++i){
    m.insert(std::make_pair(i, i*i));
    ASSERT_TRUE(m.sanity_check());
  }
  for (int i = 0; i < size; i += 2){
    m.erase(m.find(i));
    ASSERT_TRUE(m.sanity_check());
  }
  for (int i = 0; i < size; i += 2){
    ASSERT_EQ(m.find(i), m.end());
    ASSERT_NE(m.find(i+1), m.end());
    ASSERT_EQ(m.find(i+1)->first, i+1);
    ASSERT_EQ(m.find(i+1)->second, (i+1)*(i+1));
  }
}

TEST(map, erase_half_string){
  nanahan::Map<std::string, uint32_t> m;
  const uint32_t size = 200;
  char buff[8] = {};
  memcpy(buff, "buff", 4);
  for (uint32_t i = 0; i < size; ++i){
    memcpy(&buff[4], &i, 4);
    //std::cout << "insert:" << i << std::endl;
    m.insert(std::make_pair(std::string(buff), i));
    ASSERT_TRUE(m.sanity_check());
  }
  //m.dump();
  for (uint32_t i = 0; i < size; i += 2){
    memcpy(&buff[4], &i, 4);
    m.erase(m.find(std::string(buff)));
    // if(m.erase(m.find(std::string(buff))) == m.end()){
    //   std::cout << "failed to erase " <<i << std::endl;
    //   //m.dump();
    //   ASSERT_FALSE(true);
    // }
  }
  //m.dump();
  for (uint32_t i = 0; i < size; i += 2){
    memcpy(&buff[4], &i, 4);
    ASSERT_EQ(m.find(std::string(buff)), m.end());

    uint32_t j = i+1;
    memcpy(&buff[4], &j, 4);
    ASSERT_NE(m.find(std::string(buff)), m.end());
    ASSERT_EQ(m.find(std::string(buff))->first, std::string(buff));
    ASSERT_EQ(m.find(std::string(buff))->second, (i+1));
  }
}

TEST(map, size){
  nanahan::Map<int, int> m;
  const size_t size = 10000;
  for (size_t i = 0; i < size; ++i){
    ASSERT_EQ(m.size(), i);
    m.insert(std::make_pair(i, i*i));
    ASSERT_TRUE(m.sanity_check());
    ASSERT_EQ(m.size(), i+1);
  }
}

TEST(map, not_empty){
  nanahan::Map<int, int> m;
  ASSERT_TRUE(m.empty());
  m.insert(std::make_pair(2, 3));
  ASSERT_TRUE(m.sanity_check());
  ASSERT_FALSE(m.empty());
}

size_t slow_bitcount(uint64_t bits){
  size_t result = 0;
  while(bits){
    result += bits & 1;
    bits >>= 1;
  }
  return result;
}
size_t slow_bitcount(uint32_t bits){
  size_t result = 0;
  while(bits){
    result += bits & 1;
    bits >>= 1;
  }
  return result;
}

TEST(bitcount, 32){
  boost::mt19937 gen( static_cast<unsigned long>(0) );
  boost::uniform_smallint<> dst( 0, 1 << 31 );
  boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                           > rand( gen, dst );
  for(int i = 0; i < 100000; ++i){
    uint32_t r = rand();
    ASSERT_EQ(slow_bitcount(r), nanahan::detail::bitcount(r));
  }
}
TEST(bitcount, 64){
  boost::mt19937 gen( static_cast<uint32_t>(0) );
  boost::uniform_smallint<> dst( 0, 1 << 31 );
  boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                           > rand( gen, dst );
  for(int i = 0; i < 10000; ++i){
    uint64_t r = rand() * rand();
    ASSERT_EQ(slow_bitcount(r), nanahan::detail::bitcount(r));
  }
}
TEST(map, random_int){
  const int size = 4000;
  const int tries = 10;  // more and more!
  for(int j = 0; j < tries; ++j){
    std::cout << "trying seed:" << j << std::endl;
    nanahan::Map<int, int> m;
    {
      boost::mt19937 gen( static_cast<unsigned long>(j) );
      boost::uniform_smallint<> dst( 0, 1 << 30 );
      boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                               > rand( gen, dst );
      for(int i = 0; i < size; ++i){
        m.insert(std::make_pair(rand(), i));
        ASSERT_TRUE(m.sanity_check());
      }
    }

    {
      boost::mt19937 gen( static_cast<unsigned long>(j) );
      boost::uniform_smallint<> dst( 0, 1 << 30 );
      boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                               > rand( gen, dst );
      for(int i = 0; i < size; ++i){
        //std::cout << i;
        ASSERT_NE(m.find(rand()), m.end());
        //std::cout << ".";
      }
    }
  }
}
TEST(operator, copy){
  nanahan::Map<int, int> m, o;
  const int size = 10000;
  for (int i = 0; i < size; ++i){
    m.insert(std::make_pair(i, i*i));
    ASSERT_TRUE(m.sanity_check());
  }
  o = m;
  ASSERT_TRUE(m.sanity_check());
  ASSERT_TRUE(o.sanity_check());
  for (int i = 0; i < size; i += 2){
    // remove even number in m
    // deep copied o must not be related
    m.erase(m.find(i));
    ASSERT_TRUE(m.sanity_check());
  }
  for (int i = 0; i < size; i += 2){
    ASSERT_EQ(m.find(i), m.end());
    ASSERT_NE(m.find(i+1), m.end());
    ASSERT_EQ(m.find(i+1)->first, i+1);
    ASSERT_EQ(m.find(i+1)->second, (i+1)*(i+1));

    ASSERT_NE(o.end(), o.find(i));
    ASSERT_EQ(i, o.find(i)->first);
    ASSERT_EQ(i*i, o.find(i)->second);
    ASSERT_NE(o.end(), o.find(i+1));
    ASSERT_EQ(i+1, o.find(i+1)->first);
    ASSERT_EQ((i+1)*(i+1), o.find(i+1)->second);
  }
}

TEST(copy, construct){
  nanahan::Map<int, int> m;
  const int size = 10000;
  for (int i = 0; i < size; ++i){
    m.insert(std::make_pair(i, i*i));
  }
  nanahan::Map<int, int> o(m);
  for (int i = 0; i < size; i += 2){
    m.erase(m.find(i));
  }
  for (int i = 0; i < size; i += 2){
    ASSERT_EQ(m.find(i), m.end());
    ASSERT_NE(m.find(i+1), m.end());
    ASSERT_EQ(m.find(i+1)->first, i+1);
    ASSERT_EQ(m.find(i+1)->second, (i+1)*(i+1));

    ASSERT_NE(o.find(i), o.end());
    ASSERT_EQ(o.find(i)->first, i);
    ASSERT_EQ(o.find(i)->second, i*i);
    ASSERT_NE(o.find(i+1), o.end());
    ASSERT_EQ(o.find(i+1)->first, i+1);
    ASSERT_EQ(o.find(i+1)->second, (i+1)*(i+1));
  }
}

TEST(operator, equal){
  nanahan::Map<int, int> m,n,o;
  const int size = 10000;

  boost::mt19937 gen( static_cast<unsigned long>(0));
  boost::uniform_smallint<> dst( 0, 1 << 30 );
  boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                           > rand( gen, dst );
  for (int i = 0; i < size; ++i){
    const int r = rand();
    m.insert(std::make_pair(r, i));
    n.insert(std::make_pair(r, i));
    o.insert(std::make_pair(r, i+1));
  }
  EXPECT_TRUE(m==n);
  EXPECT_FALSE(m==o);
  EXPECT_FALSE(o==n);
}

TEST(map, iterator)
{
  typedef nanahan::Map<int, int> map;
  typedef map::iterator iterator;
  typedef map::const_iterator const_iterator;

  // DefaultConstructible
  {
    iterator it;
    const_iterator cit;
  }

  // CopyConstructible
  {
    map m;

    iterator it = m.begin();
    EXPECT_EQ(it, m.begin());
    iterator it2(m.end());
    EXPECT_EQ(it2, m.end());

    const_iterator cit = m.cbegin();
    EXPECT_EQ(cit, m.cbegin());
    const_iterator cit2(m.cend());
    EXPECT_EQ(cit2, m.cend());
  }

  // CopyAssignable
  {
    map m;

    iterator it;
    it = it;
    it = m.begin();
    EXPECT_EQ(it, m.begin());

    const_iterator cit;
    cit = cit;
    cit = m.cend();
    EXPECT_EQ(cit, m.cend());
  }

  // EqualityComparable+
  {
    map m;

    iterator it = m.begin();
    EXPECT_TRUE(it == it);
    EXPECT_FALSE(it != it);

    const_iterator cit = m.cbegin();
    EXPECT_TRUE(cit == cit);
    EXPECT_FALSE(cit != cit);

    EXPECT_TRUE(it == cit);
    EXPECT_TRUE(cit == it);
    EXPECT_FALSE(it != cit);
    EXPECT_FALSE(cit != it);

    EXPECT_TRUE(m.begin() == m.begin());
    EXPECT_TRUE(m.begin() == m.end());
    EXPECT_TRUE(m.end() == m.begin());
    EXPECT_TRUE(m.end() == m.end());
    EXPECT_FALSE(m.begin() != m.begin());
    EXPECT_FALSE(m.begin() != m.end());
    EXPECT_FALSE(m.end() != m.begin());
    EXPECT_FALSE(m.end() != m.end());
  }

  // dereference
  {
    map m;
    std::pair<int, int> p(1, 2);
    m.insert(p);

    iterator it = m.begin();
    EXPECT_EQ(p, *it);
    EXPECT_EQ(1, it->first);
    EXPECT_EQ(2, it->second);

    const_iterator cit = m.begin();
    EXPECT_EQ(p, *cit);
    EXPECT_EQ(1, cit->first);
    EXPECT_EQ(2, cit->second);
  }

  // increment
  {
    map m;
    m.insert(std::make_pair(1, 2));
    m.insert(std::make_pair(2, 3));
    m.insert(std::make_pair(3, 4));

    iterator it = m.begin();
    EXPECT_EQ(3, (++it)->second);
    EXPECT_EQ(3, it++->second);
    EXPECT_EQ((std::pair<int, int>(3, 4)),*it++);

    const_iterator cit = m.cbegin();
    EXPECT_EQ(3, (++cit)->second);
    EXPECT_EQ(3, cit++->second);
    EXPECT_EQ((std::pair<int, int>(3, 4)),*cit++);
  }

  // decrement
  {
    map m;
    m.insert(std::make_pair(1, 2));
    m.insert(std::make_pair(2, 3));
    m.insert(std::make_pair(3, 4));

    iterator it = m.end();
    EXPECT_EQ(4, (--it)->second);
    EXPECT_EQ(4, it--->second);
    EXPECT_EQ((std::pair<int, int>(2, 3)), *it--);

    const_iterator cit = m.cend();
    EXPECT_EQ(4, (--cit)->second);
    EXPECT_EQ(4, cit--->second);
    EXPECT_EQ((std::pair<int, int>(2, 3)), *cit--);
  }
}
