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
