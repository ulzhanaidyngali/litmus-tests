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



## LB, IRIW and CoRR Analysis

### LB Test
LB means Load Buffering.
Thread 1 reads x first and then writes y = 1. Thread 2 reads y first and then writes x = 1.
The interesting result is r1 = 1 and r2 = 1.
On x86 this result should not appear, because x86-TSO does not allow load-to-store reordering. This means the processor does not freely change the order between reading and writing in this case.

### IRIW Test
IRIW means Independent Reads of Independent Writes.
There are four threads. Thread 1 writes x = 1. Thread 2 writes y = 1. Thread 3 reads x and then y. Thread 4 reads y and then x.
The forbidden result is when two reader threads see the writes in different orders. For example, one thread sees x first, but another thread sees y first.
On x86 this should not happen, because x86-TSO has a single global store order. All cores should observe writes in the same order.

### CoRR Test
CoRR means Coherent Read-Read or Cache Coherence.
One thread writes x = 1 and then x = 2. Another thread reads x two times.
The forbidden result is r1 = 2 and r2 = 1. It means the thread first saw the newer value and then saw the older value.
This should not happen on x86 or any coherent system, because all cores must see writes to the same memory location in the same order.
