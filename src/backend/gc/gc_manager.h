//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// gc_manager.h
//
// Identification: src/backend/gc/gc_manager.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <thread>
#include <unordered_map>
#include <map>

#include "backend/common/types.h"
#include "backend/common/lockfree_queue.h"
#include "backend/common/logger.h"
#include "libcuckoo/cuckoohash_map.hh"

namespace peloton {
namespace gc {

//===--------------------------------------------------------------------===//
// GC Manager
//===--------------------------------------------------------------------===//

#define MAX_TUPLES_PER_GC 100000
#define MAX_FREE_LIST_LENGTH 100000

#define GC_PERIOD_MILLISECONDS 100

class GCManager {
 public:
  GCManager(const GCManager &) = delete;
  GCManager &operator=(const GCManager &) = delete;
  GCManager(GCManager &&) = delete;
  GCManager &operator=(GCManager &&) = delete;

  GCManager(const GCType type)
      : is_running_(true),
        gc_type_(type),
        possibly_free_list_(MAX_FREE_LIST_LENGTH) {
    StartGC();
  }

  ~GCManager() { StopGC(); }

  // Get status of whether GC thread is running or not
  bool GetStatus() { return this->is_running_; }

  void StartGC();

  void StopGC();

  void RecycleTupleSlot(const oid_t &table_id, const oid_t &tile_group_id,
                        const oid_t &tuple_id, const cid_t &tuple_end_cid);

  ItemPointer ReturnFreeSlot(const oid_t &table_id);

 private:
  void Unlink();
  void DeleteTupleFromIndexes(const TupleMetadata &);

  void ResetTuple(const TupleMetadata &);

 private:
  //===--------------------------------------------------------------------===//
  // Data members
  //===--------------------------------------------------------------------===//
  volatile bool is_running_;
  GCType gc_type_;
  LockfreeQueue<TupleMetadata> possibly_free_list_;
  std::unique_ptr<std::thread> gc_thread_;

  // TODO: use shared pointer to reduce memory copy
  cuckoohash_map<oid_t, std::shared_ptr<LockfreeQueue<TupleMetadata>>>
      recycled_map_;

  // Map of actual grabage.
  // The key is the timestamp when the garbage is identified, value is the
  // metadata of the garbage.
  // TODO: use shared pointer to reduce memory copy
  std::multimap<cid_t, TupleMetadata> garbage_map_;
};

}  // namespace gc
}  // namespace peloton
