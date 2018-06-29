#ifndef MPMC_MAP_DEFS_H
#define MPMC_MAP_DEFS_H

// moodycamel
#include "concurrentqueue/concurrentqueue.h"
using moodycamel::ConcurrentQueue;

// junction
#include <junction/ConcurrentMap_Grampa.h>
#include <junction/details/Grampa.h>
#include <turf/Heap.h>

// libcuckoo
#include <libcuckoo/cuckoohash_map.hh>

typedef junction::ConcurrentMap_Grampa<turf::u32, ConcurrentQueue<DataFragment>* > L1IndexMap;

typedef cuckoohash_map<uint32_t, ConcurrentQueue<DataFragment*>*> L1IndexHash;

#endif
