# Banker's Algorithm Resource Manager

A multithreaded resource-allocation simulator written in C that implements the **Banker's Algorithm** for deadlock avoidance.

The program models multiple customer threads competing for several resource types. It tracks each thread's maximum claim, current allocation, remaining need, and the resources currently available in the system.

The project also includes live `pthread` customers, condition-variable synchronization, and optional deadlock detection.

## Features

- Command-driven resource manager
- Multiple resource types
- Multiple customer threads
- Maximum resource claims
- Current resource allocations
- Remaining resource needs
- Safety-state detection
- Safe sequence generation
- Resource requests and releases
- Banker's Algorithm request validation
- Automatic rollback of unsafe requests
- Real concurrent customers using POSIX threads
- Mutex protection for shared state
- Condition variables for blocked customers
- Deadlock avoidance during concurrent execution
- Deadlock detection support

## Banker's Algorithm

The program maintains the following relationship for every thread:

```text
Need = Max - Allocation
```

The available resources are calculated from the total resources minus the resources currently allocated.

Before granting a request, the program checks:

1. The request does not exceed the thread's remaining need.
2. The requested resources are currently available.
3. Granting the request leaves the system in a safe state.

If the tentative allocation would make the system unsafe, the operation is rolled back and the request is denied.

## Safety Algorithm

The safety algorithm simulates thread completion without changing the real system state.

It starts with:

```text
Work = Available
```

The algorithm repeatedly searches for an unfinished thread whose remaining `Need` can be satisfied by `Work`.

When one is found, the thread is assumed to complete and release its allocation:

```text
Work = Work + Allocation[i]
```

If all threads can finish, the state is considered safe.

## Concurrent Customers

The `run` command creates one POSIX thread for each configured customer.

Each customer:

1. Requests resources one instance at a time.
2. Waits when a request cannot currently be granted safely.
3. Continues until its maximum claim is satisfied.
4. Simulates work.
5. Releases all of its resources.
6. Exits.

Shared banking state is protected using:

```c
pthread_mutex_t
```

Blocked customers wait using:

```c
pthread_cond_wait()
```

When resources are released, waiting customers are notified using:

```c
pthread_cond_broadcast()
```

This allows the implementation to avoid busy waiting while maintaining thread-safe access to shared state.

## Commands

### Configure Threads

```text
threads <count>
```

Example:

```text
threads 5
```

### Configure Resources

```text
resources <R0> <R1> ...
```

Example:

```text
resources 10 5 7
```

### Set Maximum Claim

```text
max <thread> <R0> <R1> ...
```

Example:

```text
max 0 7 5 3
```

### Set Initial Allocation

```text
alloc <thread> <R0> <R1> ...
```

Example:

```text
alloc 0 0 1 0
```

### Display State

```text
show
```

Example output:

```text
Available: 3 3 2
T0 Allocation: 0 1 0 Max: 7 5 3 Need: 7 4 3
T1 Allocation: 2 0 0 Max: 3 2 2 Need: 1 2 2
T2 Allocation: 3 0 2 Max: 9 0 2 Need: 6 0 0
T3 Allocation: 2 1 1 Max: 2 2 2 Need: 0 1 1
T4 Allocation: 0 0 2 Max: 4 3 3 Need: 4 3 1
```

### Check Safety

```text
safe
```

Possible output:

```text
SAFE: T1 T3 T4 T0 T2
```

or:

```text
UNSAFE
```

### Request Resources

```text
request <thread> <R0> <R1> ...
```

Possible results include:

```text
GRANTED
WAIT: resources unavailable
WAIT: unsafe state
ERROR: exceeds maximum claim
```

### Release Resources

```text
release <thread> <R0> <R1> ...
```

Possible results:

```text
RELEASED
```

or:

```text
ERROR: exceeds allocation
```

### Run Concurrent Customers

```text
run
```

Example runtime messages:

```text
T0 granted R0
T1 granted R1
T2 waiting R0
T0 released all
T2 granted R0
```

### Deadlock Detection

A pending request can be recorded with:

```text
pending <thread> <R0> <R1> ...
```

Then deadlock detection can be performed with:

```text
detect
```

Possible output:

```text
NO DEADLOCK
```

or:

```text
DEADLOCK: T1 T3
```

## Building

This project requires GCC and POSIX thread support.

Compile with:

```bash
gcc -o banker banker.c -pthread
```

## Running

The program reads commands from standard input.

Run interactively:

```bash
./banker
```

Run using a scenario file:

```bash
./banker < scenarios/textbook.txt
```

Additional commands can also be appended after a scenario:

```bash
printf "safe\nshow\n" | cat scenarios/textbook.txt - | ./banker
```

## Example

A classic Banker's Algorithm configuration might contain:

```text
threads 5
resources 10 5 7

max 0 7 5 3
max 1 3 2 2
max 2 9 0 2
max 3 2 2 2
max 4 4 3 3

alloc 0 0 1 0
alloc 1 2 0 0
alloc 2 3 0 2
alloc 3 2 1 1
alloc 4 0 0 2

safe
```

The system determines whether all customers can complete using the currently available resources and prints a valid safe sequence when one exists.

## Deadlock Avoidance vs. Deadlock Detection

The project demonstrates two related but different techniques.

### Deadlock Avoidance

The Banker's Algorithm prevents the system from entering an unsafe state.

A resource may be physically available but still not be granted if allocating it would eliminate every possible safe completion sequence.

### Deadlock Detection

Deadlock detection examines a system in which requests may already be blocked and determines whether a set of threads can no longer make progress.

Avoidance acts **before** entering a dangerous state, while detection analyzes the system **after** blocking relationships already exist.

## Synchronization Design

The concurrent portion of the program follows several important synchronization rules:

- Shared banking state is protected by a mutex.
- Safety checks occur while the banking state is protected.
- Threads do not sleep while holding the mutex.
- Waiting customers use condition variables instead of busy waiting.
- Resource releases wake blocked customers so they can retry their requests.
- Log messages associated with shared-state transitions are printed while the lock is held to keep output consistent.

## Concepts Demonstrated

This project demonstrates several operating-systems concepts:

- Deadlock avoidance
- Banker's Algorithm
- Safe and unsafe states
- Resource-allocation graphs
- Resource claims
- POSIX threads
- Mutexes
- Condition variables
- Blocking synchronization
- Concurrent resource management
- Deadlock detection

## Project Structure

```text
.
├── banker.c
├── scenarios/
│   ├── textbook.txt
│   └── deadlock.txt
└── README.md
```

## Technical Notes

The implementation uses fixed-size arrays with the following limits:

```text
Maximum threads:        16
Maximum resource types: 8
```

The program is intended for Linux or another environment with POSIX thread support.

## Author

Created as part of an Operating Systems programming assignment focused on resource allocation, synchronization, deadlock avoidance, and the Banker's Algorithm.
