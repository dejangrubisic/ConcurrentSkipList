# ConcurrentSkipList

A lock free concurrent skip list in C, built on C11 atomics and benchmarked with OpenMP against a serial baseline. Written for COMP522 (Multi-core Computing) at Rice University; the design and measurements are documented in the accompanying [project report (PDF)](https://dejangrubisic.github.io/pub/projects/concurrent_skiplist.pdf).

## Problem

Ordered maps are a core building block, but making one safe for many threads is hard. A global mutex serializes every operation and kills scalability, while naive fine grained locking invites deadlock and still forces threads to wait. The report identifies the two hazards that dominate a lock free design: two threads inserting at the same spot can race and silently lose a key, and physically freeing a deleted node would require atomically retargeting every pointer that leads to it, which the report judges hardly implementable without significant overhead. The original motivation was a loadmap table: a table that is initialized serially and then read and updated by many threads at once, where insert, lookup, and delete all need to stay cheap (expected O(log N)).

## Solution

This repository implements a skip list whose nodes hold an atomic item pointer and an atomic array of next pointers, one per level. Insertion finds the predecessor at every level and splices the new node in with compare and swap loops, retrying when a concurrent writer moves the neighborhood, so no thread ever holds a lock on the hot path. The predecessors for the upper levels are recorded for free during the initial descent, so the tower above level 0 is built by reusing the same procedure, a design the report walks through case by case. Deletion is logical: the item pointer is swapped to NULL, which marks the node dead while its tower stays linked. The report chooses this delete mark precisely because it leaves insert as the only operation that can change the structure of the list, sidestepping both dangling pointers and the ABA problem, and a later put with the same key simply revives the node in place. The tradeoff, noted in the report, is that dead nodes accumulate, and reclaiming them would need either a whole structure lock or a partial scheme that locks one node; the repo bundles an MCS queue lock (the fair, scalable queue based spin lock of Mellor-Crummey and Scott) for such a cleanup, but per the report no locking is used by the skip list operations themselves.

## Benefit

All three operations (put, get, delete) run in expected O(log N) time and writers never block readers or each other. The report benchmarks three workloads (100% inserts, 100% deletes, and a 50/50 insert delete mix) over keys 0 to 255, scaling from 51.2k to 1638.4k operations on 1 to 32 threads of a two socket POWER9 (ppc64le, 160 hardware threads): the parallel version beats the serial baseline on every workload, with the 50/50 mix the most expensive under contention. The report is candid that parallel efficiency drops as threads are added and points to removing the extra compare and swap that maintains the live node count as the next optimization. Correctness under contention is checked on every run: the test harness executes each workload concurrently with OpenMP and again serially on a deep copy of the list, then compares the two structures element by element and prints PASSED or FAILED next to the measured times for both runs.

## How to run

Requires GCC with OpenMP on Linux (the Makefile uses `-fopenmp` and `-lrt`).

```sh
mkdir -p bin        # make copies the binaries here
make                # builds the optimized binary "test" and "test-debug"
./test              # or: make runp
OMP_NUM_THREADS=8 ./test
```

The test binary runs three workloads (insert, delete, and a mixed random put and delete test) over 256 random keys for 6400 rounds each, printing the parallel time, the serial time, and a PASSED or FAILED verdict per workload. `make clean` removes the binaries.

## Repository structure

* `cskiplist.c`, `cskiplist.h`: the concurrent skip list (create, put, get, delete, copy, deep copy, compare, print)
* `mcs-lock.c`, `mcs-lock.h`: MCS queue lock implementation
* `test.c`: OpenMP test and benchmark harness
* `timer.h`: wall clock timer used for the measurements
* `Makefile`: build and run targets
