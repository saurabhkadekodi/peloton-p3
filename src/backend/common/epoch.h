//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// epoch.h
//
// Identification: src/backend/common/epoch.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <atomic>
#include "backend/common/types.h"
#include "backend/common/lockfree_queue.h"

namespace peloton {

#define MAX_EPOCHS_PER_THREAD 5

class Epoch {
 public:
  Epoch(const oid_t e, size_t max_epochs = MAX_EPOCHS_PER_THREAD)
      : possibly_free_list_(FREE_LIST_LENGTH),
        ref_count(0),
        id_(e),
        max_epochs_per_thread(max_epochs) {}
  // per epoch possibly free list
  LockfreeQueue<TupleMetadata> possibly_free_list_;
  // number of threads in epoch
  std::atomic<uint64_t> ref_count;
  // Function to join the epoch
  void Join();
  // Function to leave the epoch
  bool Leave();
  // Add to the epoch's possibly free list
  void AddToPossiblyFreeList(const TupleMetadata tm);
  // Return the epoch's generation id
  oid_t GetEpochId() { return id_; }

 private:
  // epoch generation id
  oid_t id_;
  // Helper fucntion to clean epochs
  bool GCHelper(oid_t clean_from_epoch, oid_t clean_till_epoch);

  // Maximum number of epochs to clean in one GC invokation (for epoch mode)
  size_t max_epochs_per_thread;
};

}  // namespace peloton
