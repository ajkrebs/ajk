#include "gc.h"
#include "stdio.h"
#include "hamt/hamt.h"
#include "header.cpp"

class scheme_key
{
public:
  const u64 key;
  const u64 value;

  scheme_key(u64 key, u64 value)
    : key(key), value(value)
  {}

  u64 hash() const
  {
    const u8* data = reinterpret_cast<const u8*>(this);
    u64 h = 0xcbf29ce484222325;
    for (u32 i = 0; i < sizeof(scheme_key); ++i && ++data)
      {
	h = h ^ *data;
	h = h * 0x100000001b3;
      }

    return h;
  }

  bool operator==(const scheme_key& k) const
  {
    return k.key == this->key
      && k.value == this->value;
  }
};


int main(void)
{
 
  const hamt<scheme_key, u64>* h =
    new ((hamt<scheme_key,u64>*)GC_MALLOC(sizeof(hamt<scheme_key, u64>)))hamt<scheme_key, u64>();
  
  const scheme_key* const s = new ((scheme_key*)GC_MALLOC(sizeof(scheme_key))) scheme_key(const_init_int(10), const_init_int(22));
  
  h = h->insert(s,q);
  bool g = h->get(s);

  printf("Hello %i", g);
  return 0;
}

