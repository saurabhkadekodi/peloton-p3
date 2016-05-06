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

class Epoch {
 public:
  Epoch(const oid_t e): possibly_free_list_(MAX_FREE_LIST_LENGTH), ref_count(0), id_(e) {}
  LockfreeQueue<TupleMetadata> possibly_free_list_; // per epoch possibly free list
  std::atomic<uint64_t> ref_count;  // number of threads in epoch
  void Join();
  bool Leave();
  //void AddToPossiblyFreeList(const TupleMetadata tm);

  oid_t GetEpochId() { return id_; }
 private:
  oid_t id_; // epoch generation id
};

}  // namespace peloton
