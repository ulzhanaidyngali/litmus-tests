## ISA2 Test (Cause Chain)
Thread 1: x=1
Thread 2: r1=x; y=1
Thread 3: reads y and x
Illegible outcome: y=1 but x=0
we see that y has been written, but x is not yet visible
On x86-TSO: IMPOSSIBLE.
Reason: x86 stores the reason.
If a thread sees y=1, then it
necessarily sees x=1.
On ARM: POSSIBLE without barriers.

## Test N6 (3 variables)
Thread 1: x=1; r1=y
Thread 2: y=1; r2=x
Illegible outcome: r1=0, r2=0
On x86-TSO: POSSIBLE.
Reason: the memory buffer is blocked
writing. Both threads read old
values ​​before the "reached" write is complete.
Very similar to the SB test.

## 2+2W Test (Two Writers)
Thread 1: x=2; y=1
Thread 2: y=2; x=1
Possible outcomes:
x=1,y=1 or x=1,y=2
or x=2,y=1 or x=2,y=2
On x86-TSO: all 4 outcomes are possible.
Reason: there is no guarantee which thread
will write the last values ​​to x and y.



## DEKKER, WRC, RWC analysis

### DEKKER
The DEKKER test shows how two threads interact with shared memory.

// Thread 1
x = 1;
r1 = y;

// Thread 2
y = 1;
r2 = x;

An interesting outcome is:

r1 = 0
r2 = 0

On x86 this behavior is possible because writes may stay temporarily inside the Store Buffer before becoming visible to other threads.
### WRC
The WRC test checks causality between reads and writes.

// Thread 1
x = 1;

// Thread 2
r1 = x;
y = 1;

// Thread 3
r2 = y;
r3 = x;

A forbidden outcome is seeing y = 1 but x = 0.

On x86 this is generally impossible because the processor preserves memory ordering.
### RWC
The RWC test studies dependency cycles between reads and writes.

// Thread 1
r1 = y;
x = 1;

// Thread 2
r2 = x;
y = 1;

The forbidden outcome would violate causality between operations.

x86 prevents this behavior because of its stronger TSO memory model.
