#include "bdwgc/include/gc.h"    // Add back in and change tags if we want to use GC
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "hamt/hamt.h"
#include <iostream>

#define CLO_TAG 0
#define CONS_TAG 1
#define INT_TAG 2
#define STR_TAG 3
#define SYM_TAG 4
#define OTHER_TAG 6
#define ENUM_TAG 7
#define CHAR_TAG 8
#define HASH_TAG 9

#define VECTOR_OTHERTAG 1
// Hashes, Sets, gen records, can all be added here

#define V_VOID 39  //32 +7 (+7 is for anything enumerable other than null)
#define V_TRUE 31  //24 +7
#define V_FALSE 15 //8  +7
#define V_NULL 0

#define MASK64 0xffffffffffffffff // useful for tagging related operations

#define GC 1

#define MEMORY_CAP_BYTES 512000000

#define ASSERT_TAG(v,tag,msg)			\
  if(((v)&7ULL) != (tag))			\
    fatal_err(msg);

#define ASSERT_NE_TAG(v,tag,msg)		\
  if(((v)&7ULL) == (tag))			\
    fatal_err(msg);

#define ASSERT_VALUE(v,val,msg)			\
  if(((u64)(v)) != (val))			\
    fatal_err(msg);

#define ASSERT_NE_VALUE(v, val, msg)		\
  if ((DECODE_INT((v))) == (val))		\
    fatal_err(msg);

#define DECODE_CLO(v) ((u64*)((v)&(7ULL^MASK64)))
#define ENCODE_CLO(v) (((u64)(v)) | CLO_TAG)

#define DECODE_CONS(v) ((u64*)((v)&(7ULL^MASK64)))
#define ENCODE_CONS(v) (((u64)(v)) | CONS_TAG)

#define DECODE_INT(v) ((s32)((u32)(((v)&(7ULL^MASK64)) >> 32)))
#define ENCODE_INT(v) ((((u64)((u32)(v))) << 32) | INT_TAG)

#define DECODE_CHAR(v) ((s32)((u32)(((v)&(7ULL^MASK64)) >> 32)))
#define ENCODE_CHAR(v) ((((u64)((u32)(v))) << 32) |CHAR_TAG)

#define DECODE_STR(v) ((char*)((v)&(7ULL^MASK64)))
#define ENCODE_STR(v) (((u64)(v)) | STR_TAG)

#define DECODE_SYM(v) ((char*)((v)&(7ULL^MASK64)))
#define ENCODE_SYM(v) (((u64)(v)) | SYM_TAG)

#define DECODE_OTHER(v) ((u64*)((v)&(7ULL^MASK64)))
#define ENCODE_OTHER(v) (((u64)(v)) | OTHER_TAG)

#define ENCODE_HASH_POINTER(v) ((u64*)((v)&(7ULL^MASK64)))
#define DECODE_HASH_POINTER(v)  (((u64)(v)) | HASH_TAG)


// some apply-prim macros for expecting 1 argument or 2 arguments
#define GEN_EXPECT1ARGLIST(f,g)			\
  u64 f(u64 lst)				\
  {						\
    u64 v0 = expect_args1(lst);			\
    return g(v0);				\
  } 

#define GEN_EXPECT2ARGLIST(f,g)					\
  u64 f(u64 lst)						\
  {								\
    u64 rest;							\
    u64 v0 = expect_cons(lst, &rest);				\
    u64 v1 = expect_cons(rest, &rest);				\
    if (rest != V_NULL)						\
      fatal_err("prim applied on more than 2 arguments.");	\
    return g(v0,v1);						\
  } 

#define GEN_EXPECT3ARGLIST(f,g)					\
  u64 f(u64 lst)						\
  {								\
    u64 rest;							\
    u64 v0 = expect_cons(lst, &rest);				\
    u64 v1 = expect_cons(rest, &rest);				\
    u64 v2 = expect_cons(rest, &rest);				\
    if (rest != V_NULL)						\
      fatal_err("prim applied on more than 2 arguments.");	\
    return g(v0,v1,v2);						\
  }

int memory_used_bytes = 0;

// No mangled names
extern "C"
{

  typedef uint64_t u64;
  typedef int64_t s64;
  typedef uint32_t u32;
  typedef int32_t s32;
  
  class scheme_key
  {
  public:
    const u64 key;

    scheme_key(u64 key)
      : key(key)
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
      return k.key == this->key;
    }
  };
  
  class scheme_value
  {
  public:
    const u64 value;

    scheme_value(u64 value)
      : value(value)
    {}

    bool operator==(const scheme_value& v) const
    {
    
      return v.value == this->value;
    }
  };
  
  // UTILS

  void fatal_err(const char* msg)
  {
    printf("library run-time error: ");
    printf("%s", msg);
    printf("\n");
    exit(1);
  }

  //if it is a cons, check arity?
  u64 expect_not_cons(u64 p)
  {
    //  ASSERT_TAG(p, CONS_TAG, "Too few arguments provided to closure.")
    return V_TRUE;
  }
  
  u64* alloc(const u64 m)
  {
#if GC
    if (GC_get_heap_size() < MEMORY_CAP_BYTES)
      {
	return (u64*)GC_MALLOC(m);
      }
    else
      {
	fatal_err("Memory cap exceeded");
      }

#else
    if (memory_used_bytes < MEMORY_CAP_BYTES)
      {
	memory_used_bytes += m;
	return (u64*)(malloc(m));
      }
    else
      fatal_err("Memory cap exceeded.");

#endif

    return 0; 
  }

  u64* make_closure (u64 size)
  {
    int s = 8;
    u64* clo = alloc(size + s);
    clo[0] = CLO_TAG;
    //     for(u64 i = 1; i <= size / 8; i++)
    //	 clo[i] = 0;
    return clo;
  } 

  void print_u64(u64 i)
  {
    printf("%lu\n", i);
  }

  u64 expect_args0(u64 args)
  {
    if (args != V_NULL)
      fatal_err("Expected value: null (in expect_args0). Prim cannot take arguments.");
    return V_NULL;
  }

  u64 expect_args1(u64 args)
  {
    ASSERT_TAG(args, CONS_TAG, "Expected cons value (in expect_args1). Prim applied on an empty argument list.")
      u64* p = DECODE_CONS(args);
    ASSERT_VALUE((p[1]), V_NULL, "Expected null value (in expect_args1). Prim can only take 1 argument.")
      return p[0];
  }

  u64 expect_cons(u64 p, u64* rest)
  {
    // pass a pair value p and a pointer to a word *rest                          
    // verifiies (cons? p), returns the value (car p) and assigns *rest = (cdr p) 
    ASSERT_TAG(p, CONS_TAG, "Expected a cons value. (expect_cons)")               
      u64* pp = DECODE_CONS(p);
    *rest = pp[1];
    return pp[0];
  }


  u64 expect_other(u64 v, u64* rest)
  {
    // returns the runtime tag value
    // puts the untagged value at *rest
    ASSERT_TAG(v, OTHER_TAG, "Expected a vector or special value. (expect_other)")
    
      u64* p = DECODE_OTHER(v);
    *rest = p[1];
    return p[0];
  }


  /////// CONSTANTS
    
    
  u64 const_init_int(s64 i)
  {
    return ENCODE_INT((s32)i);
  }

  u64 const_init_void()
  {
    return V_VOID;
  }


  u64 const_init_null()
  {
    return V_NULL;
  }


  u64 const_init_true()
  {
    return V_TRUE;
  }

    
  u64 const_init_false()
  {
    return V_FALSE;
  }

  
  //have to update print method.  
  u64 const_init_string(const char* s)
  {
    return ENCODE_STR(s);
    /*   u64* str = (u64*)alloca(2 * sizeof(u64));
    str[0] = STR_TAG;
    str[1] = (u64)s;
    return ENCODE_OTHER(str); */
  }
        
  u64 const_init_symbol(const char* s)
  {
    return ENCODE_SYM(s);
    /*  u64* str = (u64*)alloca(2 * sizeof(u64));
    str[0] = STR_TAG;
    str[1] = (u64)s;
    return ENCODE_OTHER(str); */
  }


  /////////// PRIMS

    
  ///// effectful prims:

    
  u64 prim_print_aux(u64 v) 
  {
    if (v == V_NULL)
      printf("()");
    else if ((v&7) == CLO_TAG)
      printf("#<procedure>");
    else if ((v&7) == CONS_TAG)
      {
        u64* p = DECODE_CONS(v);
        printf("(");
        prim_print_aux(p[0]);
        printf(" . ");
        prim_print_aux(p[1]);
        printf(")");
      }
    else if ((v&7) == INT_TAG)
      {
        printf("%d", (int)((s32)(v >> 32)));
      }
    else if ((v&7) == STR_TAG)
      {   // needs to handle escaping to be correct
        printf("\"%s\"", DECODE_STR(v));
      }
    else if ((v&7) == SYM_TAG)
      {   // needs to handle escaping to be correct
        printf("%s", DECODE_SYM(v));
      }
    else if ((v&7) == OTHER_TAG
             && (VECTOR_OTHERTAG == (((u64*)DECODE_OTHER(v))[0] & 7)))
      {
        printf("#(");
        u64* vec = (u64*)DECODE_OTHER(v);
        u64 len = vec[0] >> 3;
        prim_print_aux(vec[1]);
        for (u64 i = 2; i <= len; ++i)
	  {
            printf(",");
            prim_print_aux(vec[i]);
	  }
        printf(")");
      }
    else
      printf("(print.. v); unrecognized value %lu", v);
    //...
    return V_VOID; 
  }

  u64 prim_print(u64 v) 
  {
    if (v == V_NULL)
      printf("'()");
    else if ((v&7) == CLO_TAG)
      printf("#<procedure>");
    else if ((v&7) == CONS_TAG)
      {
        u64* p = (u64*)(v&(7ULL^MASK64));
        printf("'(");
        prim_print_aux(p[0]);
        printf(" . ");
        prim_print_aux(p[1]);
        printf(")");
      }
    else if ((v&7) == INT_TAG)
      {
        printf("%d", ((s32)(v >> 32)));
      }
    else if ((v&7) == STR_TAG)
      {   // needs to handle escaping to be correct
        printf("\"%s\"", DECODE_STR(v));
      }
    else if ((v&7) == SYM_TAG)
      {   // needs to handle escaping to be correct
        printf("'%s", DECODE_SYM(v));
      }
    else if ((v&7) == OTHER_TAG
             && (VECTOR_OTHERTAG == (((u64*)DECODE_OTHER(v))[0] & 7)))
      {
        printf("#(");
        u64* vec = (u64*)DECODE_OTHER(v);
        u64 len = vec[0] >> 3;
        prim_print(vec[1]);
        for (u64 i = 2; i <= len; ++i)
	  {
            printf(",");
            prim_print(vec[i]);
	  }
        printf(")");
      }
    else
      printf("(print v); unrecognized value %lu", v);
    //...
    return V_VOID; 
  }
  GEN_EXPECT1ARGLIST(applyprim_print,prim_print)


  u64 prim_halt(u64 v) // halt
  {
    prim_print(v); // display the final value
    printf("\n");
    exit(0);
    return V_NULL; 
  }

  u64 applyprim_vector(u64 lst)
  {
    // pretty terrible, but works
    u64* buffer = (u64*)alloc(512*sizeof(u64));
    u64 l = 0;
    while ((lst&7) == CONS_TAG && l < 512) 
      buffer[l++] = expect_cons(lst, &lst);
    u64* mem = alloc((l + 1) * sizeof(u64));
    mem[0] = (l << 3) | VECTOR_OTHERTAG;
    for (u64 i = 0; i < l; ++i)
      mem[i+1] = buffer[i];
    delete [] buffer;
    return ENCODE_OTHER(mem);
  }

  u64 prim_make_45vector(u64 lenv, u64 iv)
  {
    ASSERT_TAG(lenv, INT_TAG, "first argument to make-vector must be an integer")
    
      const u64 l = DECODE_INT(lenv);
    u64* vec = (u64*)alloc((l + 1) * sizeof(u64));
    vec[0] = (l << 3) | VECTOR_OTHERTAG;
    for (u64 i = 1; i <= l; ++i)
      vec[i] = iv;
    return ENCODE_OTHER(vec);
  }
  GEN_EXPECT2ARGLIST(applyprim_make_45vector, prim_make_45vector)


  u64 prim_vector_45ref(u64 v, u64 i)
  {
    ASSERT_TAG(i, INT_TAG, "second argument to vector-ref must be an integer")
      ASSERT_TAG(v, OTHER_TAG, "first argument to vector-ref must be a vector")

      if ((((u64*)DECODE_OTHER(v))[0]&7) != VECTOR_OTHERTAG)
        fatal_err("vector-ref not given a properly formed vector");

    return ((u64*)DECODE_OTHER(v))[1+(DECODE_INT(i))];
  }
  GEN_EXPECT2ARGLIST(applyprim_vector_45ref, prim_vector_45ref)


  u64 prim_vector_45set_33(u64 a, u64 i, u64 v)
  {
    ASSERT_TAG(i, INT_TAG, "second argument to vector-ref must be an integer")
      ASSERT_TAG(a, OTHER_TAG, "first argument to vector-ref must be an integer")

      if ((((u64*)DECODE_OTHER(a))[0]&7) != VECTOR_OTHERTAG)
        fatal_err("vector-ref not given a properly formed vector");

        
    ((u64*)(DECODE_OTHER(a)))[1+DECODE_INT(i)] = v;
        
    return V_VOID;
  }
  GEN_EXPECT3ARGLIST(applyprim_vector_45set_33, prim_vector_45set_33)


  ///// void, ...

    
  u64 prim_void()
  {
    return V_VOID;
  }

  ///// eq?, eqv?, equal?

    
  u64 prim_eq_63(u64 a, u64 b)
  {
    if (a == b)
      return V_TRUE;
    else
      return V_FALSE;
  }
  GEN_EXPECT2ARGLIST(applyprim_eq_63, prim_eq_63)


  u64 prim_eqv_63(u64 a, u64 b)
  {
    if (a == b)
      return V_TRUE;
    //else if  // optional extra logic, see r7rs reference
    else
      return V_FALSE;
  }
  GEN_EXPECT2ARGLIST(applyprim_eqv_63, prim_eqv_63)

  /*
    u64 prim_equal_63(u64 a, u64 b)
    {
    return 0;
    }
    GEN_EXPECT2ARGLIST(applyprim_equal_63, prim_equal_63)
  */


  ///// Other predicates


  u64 prim_number_63(u64 a)
  {
    // We assume that ints are the only number
    if ((a&7) == INT_TAG)
      return V_TRUE;
    else
      return V_FALSE;
  }
  GEN_EXPECT1ARGLIST(applyprim_number_63, prim_number_63)


  u64 prim_integer_63(u64 a)
  {
    if ((a&7) == INT_TAG)
      return V_TRUE;
    else
      return V_FALSE;
  }
  GEN_EXPECT1ARGLIST(applyprim_integer_63, prim_integer_63)


  u64 prim_void_63(u64 a)
  {
    if (a == V_VOID)
      return V_TRUE;
    else
      return V_FALSE;
  }
  GEN_EXPECT1ARGLIST(applyprim_void_63, prim_void_63)


  u64 prim_procedure_63(u64 a)
  {
    if (((a)&7ULL) == (OTHER_TAG))
      {
	u64* v = DECODE_OTHER(a);
	return v[0] == CLO_TAG;
      }
    else
      {
	return V_FALSE;
      }
    //  u64* v = DECODE_OTHER(a);
    //  if (v[0] == CLO_TAG)
    //   return V_TRUE;
    //  else
    // return V_FALSE;
  }
  GEN_EXPECT1ARGLIST(applyprim_procedure_63, prim_procedure_63)

  ///// null?, cons?, cons, car, cdr


  u64 prim_null_63(u64 p) // null?
  {
    if (p == V_NULL)
      return V_TRUE;
    else
      return V_FALSE;
  }
  GEN_EXPECT1ARGLIST(applyprim_null_63, prim_null_63)    


  u64 prim_cons_63(u64 p) // cons?
  {
    if ((p&7) == CONS_TAG)
      return V_TRUE;
    else
      return V_FALSE;
  }
  GEN_EXPECT1ARGLIST(applyprim_cons_63, prim_cons_63)    


  u64 prim_cons(u64 a, u64 b)
  {
    u64* p = alloc(2*sizeof(u64));
    p[0] = a;
    p[1] = b;
    return ENCODE_CONS(p);
  }
  GEN_EXPECT2ARGLIST(applyprim_cons, prim_cons)


  u64 prim_car(u64 p)
  {
    u64 rest;
    u64 v0 = expect_cons(p,&rest);
    
    return v0;
  }
  GEN_EXPECT1ARGLIST(applyprim_car, prim_car)


  u64 prim_cdr(u64 p)
  {
    u64 rest;
    u64 v0 = expect_cons(p,&rest);
    
    return rest;
  }
  GEN_EXPECT1ARGLIST(applyprim_cdr, prim_cdr)


  ///// s32 prims, +, -, *, =, ...

    
  u64 prim__43(u64 a, u64 b) // +
  {
    ASSERT_TAG(a, INT_TAG, "(prim + a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim + a b); b is not an integer")

      //printf("sum: %d\n", DECODE_INT(a) + DECODE_INT(b));
    
      return ENCODE_INT(DECODE_INT(a) + DECODE_INT(b));
  }

  u64 applyprim__43(u64 p)
  {
    if (p == V_NULL)
      return ENCODE_INT(0);
    else
      {
        ASSERT_TAG(p, CONS_TAG, "Tried to apply + on non list value.")
	  u64* pp = DECODE_CONS(p);
        return ENCODE_INT(DECODE_INT(pp[0]) + DECODE_INT(applyprim__43(pp[1])));
      }
  }
    
  u64 prim__45(u64 a, u64 b) // -
  {
    ASSERT_TAG(a, INT_TAG, "(prim + a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim - a b); b is not an integer")
    
      return ENCODE_INT(DECODE_INT(a) - DECODE_INT(b));
  }

  u64 applyprim__45(u64 p)
  {
    if (p == V_NULL)
      return ENCODE_INT(0);
    else
      {
        ASSERT_TAG(p, CONS_TAG, "Tried to apply - on non list value.")
	  u64* pp = DECODE_CONS(p);
        if (pp[1] == V_NULL)
	  return ENCODE_INT(0 - DECODE_INT(pp[0]));
        else // ideally would be properly left-to-right
	  return ENCODE_INT(DECODE_INT(pp[0]) - DECODE_INT(applyprim__43(pp[1])));
      }
  }
    
  u64 prim__42(u64 a, u64 b) // *
  {
    ASSERT_TAG(a, INT_TAG, "(prim * a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim * a b); b is not an integer")
    
      return ENCODE_INT(DECODE_INT(a) * DECODE_INT(b));
  }

  u64 applyprim__42(u64 p)
  {
    if (p == V_NULL)
      return ENCODE_INT(1);
    else
      {
        ASSERT_TAG(p, CONS_TAG, "Tried to apply + on non list value.")
	  u64* pp = DECODE_CONS(p);
        return ENCODE_INT(DECODE_INT(pp[0]) * DECODE_INT(applyprim__42(pp[1])));
      }
  }
    
  u64 prim__47(u64 a, u64 b) // /
  {
    ASSERT_TAG(a, INT_TAG, "(prim / a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim / a b); b is not an integer")
      ASSERT_NE_VALUE(b, 0, "(prim / a b); b is 0: Division by 0 undefined.")
   
      
      return ENCODE_INT(DECODE_INT(a) / DECODE_INT(b));
  }
    
  u64 prim__61(u64 a, u64 b)  // =
  {
    ASSERT_TAG(a, INT_TAG, "(prim = a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim = a b); b is not an integer")
        
      if ((s32)((a&(7ULL^MASK64)) >> 32) == (s32)((b&(7ULL^MASK64)) >> 32))
        return V_TRUE;
      else
        return V_FALSE;
  }

  u64 prim__60(u64 a, u64 b) // <
  {
    ASSERT_TAG(a, INT_TAG, "(prim < a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim < a b); b is not an integer")
    
      if ((s32)((a&(7ULL^MASK64)) >> 32) < (s32)((b&(7ULL^MASK64)) >> 32))
        return V_TRUE;
      else
        return V_FALSE;
  }

  u64 prim__60_61(u64 a, u64 b) // <=
  {
    ASSERT_TAG(a, INT_TAG, "(prim <= a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim <= a b); b is not an integer")
        
      if ((s32)((a&(7ULL^MASK64)) >> 32) <= (s32)((b&(7ULL^MASK64)) >> 32))
        return V_TRUE;
      else
        return V_FALSE;
  }

  u64 prim__62(u64 a, u64 b) // >
  {
    ASSERT_TAG(a, INT_TAG, "(prim < a b); a is not an integer")
      ASSERT_TAG(b, INT_TAG, "(prim < a b); b is not an integer")
    
      if ((s32)((a&(7ULL^MASK64)) >> 32) > (s32)((b&(7ULL^MASK64)) >> 32))
        return V_TRUE;
      else
        return V_FALSE;
  }

  //Added check to throw an error is a is not a boolean value.
  u64 prim_not(u64 a) 
  {
    if (a == V_FALSE)
      return V_TRUE;
    else if (a == V_TRUE)
      return V_FALSE;
    else
      fatal_err("prim not (a): a is not a boolean value");

    return V_NULL;
  }
  GEN_EXPECT1ARGLIST(applyprim_not, prim_not)

  //Part III prims
  
  u64 prim_hash()
  {
    const hamt<scheme_key, scheme_value>* h =new ((hamt<scheme_key, scheme_value>*)
						  alloc(sizeof(hamt<scheme_key, scheme_value>)))hamt<scheme_key, scheme_value>();   
    u64* hash = (u64*)alloc(2 * sizeof(u64));
    hash[0] = (7 << 3) | HASH_TAG; 
    hash[1] = (u64)h;
    return ENCODE_OTHER(hash);
    
  } 
  
  u64 prim_hash_45set(u64 h, u64 key, u64 value)
  {
    u64* hash_object = DECODE_OTHER(h);
    const scheme_key* const k = new ((scheme_key*)alloc(sizeof(scheme_key))) scheme_key(key);
    const scheme_value* const v = new ((scheme_value*)alloc(sizeof(scheme_value))) scheme_value(value);
    const hamt<scheme_key, scheme_value>* hash_ptr = (hamt<scheme_key, scheme_value>*)hash_object[1];
    hash_ptr = hash_ptr->insert(k, v);
    hash_object[1] = (u64)hash_ptr;
    return ENCODE_OTHER(hash_object);
  }

  u64 prim_hash_45remove(u64 h, u64 key)
  {
    u64* hash_object = DECODE_OTHER(h);
    const scheme_key* const k = new ((scheme_key*)alloc(sizeof(scheme_key))) scheme_key(key);
    const hamt<scheme_key, scheme_value>* hash_ptr = (hamt<scheme_key, scheme_value>*)hash_object[1];
    hash_ptr = hash_ptr->remove(k);
    hash_object[1] = (u64)hash_ptr;
    return ENCODE_OTHER(hash_object);
  }

  //ToDo:
  //return key if it exists in the hash. If not,
  //emit some symbol to tell the wrapper to call
  //the thunk.
  u64 prim_hash_45ref(u64 h, u64 key)
  {
    u64* hash_object = DECODE_OTHER(h);
    const hamt<scheme_key,scheme_value>* hash_ptr = (hamt<scheme_key,scheme_value>*)hash_object[1];
    const scheme_key* const s = new ((scheme_key*)alloc(sizeof(scheme_key))) scheme_key(key);
    const scheme_value* v = hash_ptr->get(s);
    if (v == 0) return const_init_int(0); else return v->value;
  }

  u64 prim_hash_45count(u64 h)
  {
    u64* hash_object = DECODE_OTHER(h);
    const hamt<scheme_key,scheme_value>* hash_ptr = (hamt<scheme_key,scheme_value>*)hash_object[1];
    return const_init_int(hash_ptr->size());
  }
}

/*
int main(void)
{
  u64 h = prim_hash();
   u64 v1 = const_init_int(0), v2 = const_init_int(2),
    vl = const_init_int(3), v3 = const_init_int(4),
    v4 = const_init_int(5), v5 = const_init_int(6),
    v6 = const_init_int(7), v7 = const_init_int(8),
    v8 = const_init_int(9);
  h = prim_hash_45set(h, v1, v2);
  h = prim_hash_45set(h, v3, v4);
  h = prim_hash_45set(h, v5, v6);
  h = prim_hash_45set(h, v7, v8);
  h = prim_hash_45set(h, const_init_int(8), const_init_int(9));
  u64 v = prim_hash_45ref(h, const_init_int(8));
  printf("size: %i\n", DECODE_INT(prim_hash_45count(h)));
  printf("value: %i\n", DECODE_INT(v));
  h = prim_hash_45remove(h, const_init_int(8));
  u64 v0 = prim_hash_45ref(h, const_init_int(8));
  printf("size: %i\n", DECODE_INT(prim_hash_45count(h)));
  printf("value0: %i\n", DECODE_INT(v0)); 

} 

*/
