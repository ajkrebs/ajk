

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
