/// Notes on what works ////
I had everything working with both gc on and off, then my computer crased and I must
have made some chages that didn't get saved. I came back after I got everything
booted and for some reason I couldn't get "guard-continuations" or "exceptions-continuations"
to run without seg-faulting. I really have no idea why they don't work, and I don't have
time to fix them. 

//// Description of Project ////

The compiler takes in a program written in the subset of Scheme satisfying the top-level-exp? predicate.
It first completes a desugar pass to remove all nested begins while desugaring defines to letrec*'s.

It then performs a second desugar pass that simplifies the langauge further by desugaring all let forms
(let*, letrec, letrec*, let loop, etc.) into plain let forms. It also desugars promises into an explicit
implementation, removing the delay and force forms. Boolean forms and, or and if and conditional forms unless and
when are desugared into if statements. Finally, the dynamic-wind and the guard and raise forms
are desugared into calls to call/cc with added added functionality to handle exceptions.

The final simplification of the input language removes several primitive
operations while also wrapping the input program in explicit implementations of
hose prims. It also simplifies how some primitive operations are performed.

After the final simplification is completed, the program is assignment converted. Here, the
set! form is removed from the language by explicitly allocating all mutable local variables on the
heap by boxing them, which entails wrapping them in vectors of length 1 which are stored
on the heap. The language is then alphatized, removing variable shadowing and making it so
each variable in the program is unique.

Once the program has been alphatized and assignment converted, it is then
converted to administrative-normal form (ANF). In ANF, the language is separated into two forms,
complex experssions and atomic expresssions. Atomic expressions are experssions that can be evaluated
immediately, or in a finite amout of time. Complex expressions are expressions which can take and arbitrairy amount
of time to evaluate. Also in anf, all let forms with multiple right hand sides are converted to let forms
with only one right hand side. All complex sub expressions are also removed, which is neccessary for LLVM, the final
ir, which does not allow for comlex sub expressions. To do this, we break all sub expressions up and let bind
the intermediate computations to temp variables, enforcing an explicit order of evaluation on expressions in the process. 

The program in ANF is then converted to continuation-passing style (CPS). In CPS, we carry around
a continuation, which is essentially the stack wrapped in a function. Programs now never return, and
return points are now written as applications of continuations. As a result, all functions must
take an extra argument to enable this behavior, with the extra argument being the current continuation.
In CPS, the call/cc form can now be written as a simple application of the continuation.

After the program is in CPS form, it is then closure converted. In closure converted form, all lambda
abstractions are converted into structures similar to C-style procedures and hoisted to the top level.
These structures contain a function pointer and an environment mapping that holds the free
variables needed for evaluation of that function, which are stored as a flat vector.
Closure conversion is performed using a bottom-up strategy, where we first traverse the AST all the
way down to the terminals. Then, we compute a set of free variables on the way back up. At each lambda
abstraction, we emit a closure object, a flat vector that contains a function pointer to the current
lambda and a set of free variables. All free variables in inner lambdas are propagated up into
outer lambdas. The body of the initial program is denoted as the main procedure. 

Finally, the program is converted from closure converted form into LLVM. All of the top level
lambda abstractions are now converted to LLVM functions. 

//// Supported Primitive Operations ////

// Arithmetic Operators //

/ + /

Takes in zero or more integer arguments. If any argument is not an integer, it
will fail with an error. Returns the sum of the arguments.

/ - /

Takes in any zero or more integer arguments. If any argument is not an integer,
it will fail with an error. Returns the difference of the arguments. 

/ * /

Takes in zero or more integer arguments. If any argument is not an integer,
it will fail with an error. Returns the product of the arguments. 

/ = /

Takes in zero or more integer arguments. If any argument is not an integer,
it will fail with an error. Returns true if all values are numerically equal,
false otherwise. 

/ < /

Takes in any number of integer arguments. 

/ > /

// Lists //

/ car /

Takes in a single argument. The argument must be a non-null list or a pair, any other type will
produce an error. Returns the first element of the pair or list. 

/ cdr /

Takes in a single argument. The argument must be a non-null list or a pair, any other type will
produce an error. Returns the second element of the pair, or the tail of the list. 

/ cons /

Takes in two arguments of any type. Returns the two arguments as a pair. 

/ append /

Takes in zero or more arguments. All arguments must be of type list. If any argument
is not a list, it will fail with an error. Returns the result of appending all subsequent
lists to the end of the first list. 

/ null? /

Takes in a single argument. The argument can be of any type. Returns true if the
argument is structurally equal to the Scheme null value '().

Other

/ promise? /

Takes in a single argument. The argument can be of any type. Returns true
if the argument is of type promise, i.e. it was previously returned from
a call to delay.

////         Part 2             ////

/// List of Handled run-time errors ///

// Division by zero //

Exits the program with a value of 1 after printing the string:

	"library runtime error: (prim / a b); b is 0: Division by 0 undefined.")

This error is handled in header.cpp in the prim__47() function. It simply
checks if b is 0.


// Function is provided too many arguments //

Halts the program with the error message

      "library runtime error: Too few arguments provided to function."

This error is handled in closure-convert.rkt in the remove-varargs function.
Since every lambda takes in a cons list of arguments, I just added a check
to make sure the entire list is consumed by the lambda, i.e. if there
is anything left over in the argument list then too many args were provided.


// Function is provided too few arguments //  

Halts the program with the error message

      "library runtime error: Too few arguments provided to function."

This error is handled in closure-convert.rkt in the remove-varargs function.
Since every lambda takes in a cons list of arguments, I just added a check
to make sure the list is not null after each element of the argument list
is consumed. If any nulls are found, then too few arguments were provided
to the function.


// Memory cap of 512 MB //

Exits the program with value 1 after printing the string:

      "library runtime error: Memory cap exceeded"

I handled this error in header.cpp. I added a global variable that keeps track
of the current amount of memory allocated in bytes by adding to that value
for each call to alloc. When the toggle for garbage collection is on, it
compares the value GC_get_heap_size() to the memory cap.

Note: I was having trouble testing this case because my VM was hitting its
memory cap and becoming extremely slow.

// Various Error Fixes //
I added a check to prim__not to throw an error if an non-boolean value is passed in.


I didn't get to handling a fifth error. I also didn't get to fix the error
reporting in tests.rkt and utils.rkt.


//// Part III ////

/// HAMT Implementation ///

For part III and IV I was compiling using the flags -lgc and -std=c++11.
I couldn't get the gc to work with an explicit path, but I found that
-lgc fixed the problem. 

For part 3 I added support for the functional hash map using
the given HAMT implementation. I made two classes, scheme_key
and scheme_value which would act as a key/value types for the
hash. I then added a hash function, which I got from the test_hamt.cpp
file, to the scheme_key class.

The hash tagging scheme works in the same way as the vector tagging scheme.
I just made an array of length 2 and stored a tag in the first element and a
pointer to the actual HAMT object, casted to u64, in the second slot. I then
implemented prim operations hash-ref, hash-set, hash-count, and hash using the
similar methods in the HAMT implementation. For hash-ref, I added some extra functionality
to allow for the behavior of a thunk to be called when a value is not found. To do this,
in desugar.rkt I matched on two cases for hash-ref, (prim hash-ref h k t) and
(prim hash-ref h k) and desugared them into a call to a hash-ref internal procedure.
I wrapped the desugared output in an implementation of this procedure, which just
calls hash-ref and compares the resuting value to 0. If the value is 0, then the
value was not found, so it calls the thunk. If the value is not found and thunk is
not provided, it will halt with an error "library runtime error: Hash key not found."

The hash works in simple cases, when integers are given as keys for example. However,
when other objects, like Strings or Symbols, are given as keys only insertion
and count work, ref and remove do not work.

//// Part IV ////

I used the same flags as above to compile the programs for part IV.

I managed to get all of the programs to compile with garbage collection enabled, aside
from those noted above.
I added a #define GC that toggles garbage collection on and off in the header.cpp file. To get GC
working, I wrapped all registre allocations in calls to alloca and store. I did this
by modifying closure-convert.rkt. I just wrote a function that would wrap a call
to allocate a register in calls to alloca and store.

I also changed the way I was handling closures. I made a make_closure function that would allocate
an array of size 8 + the given closure size. I then stuck the closure tag in the 0th
element. To get this to work with the rest of the programs, I just offset all
accesses to closure objects by 1. So, all function pointers were located at
index 1, instead of index 0, and all free variables were found in the following
indices.

While the programs compiled and ran fine with the garbage collector enabled, I was
getting some memory errors when running the binary through valgrind. Specifically,
I was getting use of uninitialized memory errors and branching on uninitialized
values errors. 
