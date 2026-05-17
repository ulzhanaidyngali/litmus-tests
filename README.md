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
The DEKKER test demonstrates how two threads interact with shared memory on modern processors.
Thread 1 writes the value 1 to variable x and then reads variable y.
Thread 2 writes the value 1 to variable y and then reads variable x.
// Thread 1
x = 1;
r1 = y;
// Thread 2
y = 1;
r2 = x;
An interesting outcome is:
r1 = 0
r2 = 0
This means that both threads failed to observe the other thread’s write operation even though both writes already happened.
On x86 processors this behavior is possible because of the Store Buffer mechanism.
Writes are temporarily stored inside the processor before becoming visible to other cores. Because of this delay, both threads may still read the old value 0.
The DEKKER test demonstrates weak memory behavior and shows how hardware optimizations affect memory visibility and synchronization.
### WRC
The WRC test checks whether the processor preserves causality between write and read operations.
One thread writes x = 1.
Another thread reads x and then writes y = 1.
The third thread reads both y and x.
// Thread 1
x = 1;
// Thread 2
r1 = x;
y = 1;
// Thread 3
r2 = y;
r3 = x;
A forbidden outcome is:
r2 = 1
r3 = 0
This result would violate causality because if a thread already observed x = 1 before writing y = 1, then any thread that sees y = 1 should also observe x = 1.
On x86-TSO this outcome is generally impossible because the architecture preserves a relatively strong ordering of writes and memory visibility.
### RWC
The RWC test studies cyclic dependencies between reads and writes in concurrent execution.
Threads perform reads and writes in a way that creates a dependency cycle between operations.
// Thread 1
r1 = y;
x = 1;
// Thread 2
r2 = x;
y = 1;
A forbidden outcome occurs when both threads simultaneously observe values that should not yet be visible.
On x86 processors this behavior is not allowed because the TSO memory model prevents invalid cyclic dependencies between memory operations.
The RWC test demonstrates that x86 maintains causality and stronger memory ordering guarantees compared to weaker architectures such as ARM.
