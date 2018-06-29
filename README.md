# mpmc_example
Sandbox development of a threading model for sROD-like DAQ for mm_ddaq

# Requirements
* boost
* a recent C(++) compiler

# Installation

In addition to the requirements above, there is an executable that checks out and installs further sw requirements.

The additional libraries that are needed are related to concurrent queue and hash map implementation as well as 
a logging library:

1) [spdlog](https://github.com/gabime/spdlog) : logging library
2) [libcuckoo](https://github.com/efficient/libcuckoo) : a nice (re:fast) concurrent hash map implementation
3) [junction+turf](https://preshing.com/20160201/new-concurrent-hash-maps-for-cpp/) : a rawer (re:faster) concurrent hash map implementation
4) [concurrentqueue](https://github.com/cameron314/concurrentqueue) : a very nice lock-free and mulit-producer, multi-consuer concurrentqueue implementation

Run the following to get these dependencies and compile everything:

```bash
 git clone https://github.com/dantrim/mpmc_example
 cd mpmc_example
 ./get_dependencies
 mkdir build
 cmake ..
 make
```

If the **make** command completes successfully, you will have the *sender* and *mpmc_test* executables.

# Description

This package builds two executables:

1) *sender* : simply spins up a couple of threads which continuously send arbitrary data over network (UDP) in loopback to some hardcoded IP:ports

2) *mpmc_test* : sandboxing a few classes for developing DAQ dataflow from multiple inputs (listeners), indexing them, and outputing them in a single output queue
