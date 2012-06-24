
Map
===
Usage is similar to boost/std::tr1 unordred_map.

```
#include <assert.h>
#include "nanahan/map.hpp"
int main(void)
{
  nanahan::Map<int, int> m;
  for (int i = 0; i<1000; ++i){
    m.insert(std::make_pair(i*10, i-10));
  }

  nanahan::Map<int, int> m;
  for (int i = 0; i<1000; ++i){
    assert(m.find(i*10) != m.end());
    assert(m.find(i*10 + 1) == m.end());
  }

  for (int i = 0; i<1000; ++i){
    m.erase(i*10);
  }
}
```

![insert_bench](https://github.com/kumagi/nanahan/blob/master/map/insert_graph.png?raw=true)
![find_bench](https://github.com/kumagi/nanahan/blob/master/map/find_graph.png?raw=true)
![erase_bench](https://github.com/kumagi/nanahan/blob/master/map/erase_graph.png?raw=true)
