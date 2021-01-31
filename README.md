CacheTracer
===========

Intel PIN Simulator extensions to simulate different many level caches and different jump predictors. It is
mainly a project to study different performance improvements employed in the microarchitecture of CPU's.

Goal
======
Implement by software some of the mechanisms in use in CPU microarchitectures of modern processors,
so we can evaluate the performance of these tools synthetically, without having to build them.

The project was focused on two of the most pressing issues considering microarchitectural performance,
the cache hit rate and the jump predictor.

Both simulators were built around Intel tools, using the PIN library (http://www.pintool.org/)


Compiling
===========
To build the program:

1. Download `pin` from Intel official site (only tested in linux)
https://software.intel.com/content/www/us/en/develop/articles/pin-a-binary-instrumentation-tool-downloads.html

2. Extract the tar.gz to a folder like `~/pin`

3. Set `export PIN_ROOT=~/pin` in your bash

4. Build the simulator: `pushd CacheSim; make all; popd; pushd JmpSim; make all; popd`


Multilevel cache simulator
================

a) To run:

  `$(PIN_ROOT)/pin -t CacheSim/obj-intel64 <parameters> -- <executable>`

i.e to run "ls -la" using a 6-way LRU Associative cache:

  `$(PIN_ROOT)/pin -t CacheSim/obj-intel64 -V 6 -lru -1 -o cache.out -- <executable>`

and results can be seen in "cache.out"


Possible parameters are:

-L   [default 4]
	Specify the amount of bits a memory address has to be used to index inside the line
	(line size)

-L1  [default 16]
	Specify the amount of bits an L1 address has. (cache size)

-L2  [default 20]
	Specify the amount of bits an L1 address has. (cache size)

-V   [default 4]
	Specify the amount of ways a cache line has.

-lru [default 0]
	Use LRU instead of FIFO as a line replacement policy.

-o   [default cache.out]
	Specify the report output filename.


b) Details

The cache simulator code can be found in:
  CacheSim/CacheSim.cpp

The simulator is designed to keep an equivalent of a writeback cache memory, in RAM.

That is, it has the necessary data structures to be a cache (present bit, dirty bit and assigned tag to that line)

There is an only function to access the cache, "accessCache", and its parameters indicate if it is a write or a read,
and what in what cache level it has to look up that data.

This function takes care to address theline in the accessed cache level and sees if it matches the tag in every line.
If it doesn't, it tries to read in an upper level to see if it finds it. If there are no more levels, it counts a miss and
makes as if it has to look for that data in RAM. Then, if needed, it releases a way in that line and kees the data in the lower cache.
This allows the next access to be in the L1, improving efficiency.

If the data is found at some level, it simply records that hit.

c) Notes on the cache simulator

The simulator is designed to be completely extensible to the necessary cache levels, from one to five or more, if needed.
The only changed need would be to add the extra parameters to be able to customize them.hacer facilmente.


Jump Predictor Simulator
===================

a) To run:

  `$(PIN_ROOT)/pin -t JmpSim/obj-intel64 <parameters> -- <executable>`

i.e, to run "ls -la" with a BHT of 2^4 entries:

  `$(PIN_ROOT)/pin -t JmpSim/obj-intel64  -s 4 -o cache.out -- ls -la`

Report results are, by default, in "jmp.out"


Possible parameters are:

-s   [default 12]
	Specifies the amount of bits in the BHT table (size, amount of bits of a memory address
	used to index a state machine table.)

-o   [default jmp.out]
	Specifies the report filename.

b) Details
The jump predictor simulator code is at:
	JmpSim/JmpSim.cpp

The simulators was conceived to try to understand basic jump prediction mechanisms,
to avoid pipeline stalls when conditional branches arise.

The internal mechanism is simple. A binding to the program passed by arguments
is created using PIN, so the simulator counts every time a conditional instruction is executed.

Several prediction mechanisms were implemented, some of the obvious and stateless,
"Always Jump" and "Never Jumps" as proof of how jumps in a program are located.
Some of the more interesting ones such as "Jump if and only if going to a lower address" proved
to be much more effective than naive ones, reasonable performance for adding little additional logic.

Two more advanced ones, '2 bit sat counter' and '2 bit hystheresis' were also implemented. They
work by state machines indexed by some bits of the instruction point.

Some of these methods were discussed in the  Henessy-Patterson book, "Computer Architecture".


Interesting programs to try
================
* ls -la
* traceroute google.com
* bash (and use it normally, quit by logout)
* firefox
* locate file"  (even if it doesn't exist, has to use sudo for it)
* ssh \<user\>@\<machine\>

Notes
=====

The folder "simulators" has to be placed in an accesible place to the PIN compiler files.

Example output can be found in "cache.out" and "jmp.out". It is in spanish so far, but it can be translanted easily.
