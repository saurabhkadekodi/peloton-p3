//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// transaction_manager.h
//
// Identification: src/backend/concurrency/transaction_manager.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <atomic>
#include <unordered_map>
#include <list>

#include "backend/common/platform.h"
#include "backend/common/types.h"
#include "backend/concurrency/transaction.h"
#include "backend/concurrency/epoch_manager.h"
#include "backend/storage/data_table.h"
#include "backend/storage/tile_group.h"
#include "backend/storage/tile_group_header.h"
#include "backend/catalog/manager.h"
#include "backend/expression/container_tuple.h"
#include "backend/storage/tuple.h"
#include "backend/gc/gc_manager_factory.h"
#include "backend/common/epoch.h"

#include "libcuckoo/cuckoohash_map.hh"

namespace peloton {
namespace concurrency {

extern thread_local Transaction *current_txn;
extern thread_local Epoch *current_epoch;

#define RUNNING_TXN_BUCKET_NUM 10

class TransactionManager {
 public:
  TransactionManager() {
    next_txn_id_ = ATOMIC_VAR_INIT(START_TXN_ID);
    next_cid_ = ATOMIC_VAR_INIT(START_CID);
    cid_of_smallest_epoch_cleaned_ = ATOMIC_VAR_INIT(START_CID);
  }

  virtual ~TransactionManager() {}

  txn_id_t GetNextTransactionId() { return next_txn_id_++; }

  cid_t GetNextCommitId() { return next_cid_++; }

  oid_t GetNextEpochId() { return next_epoch_id_++; }
  bool IsOccupied(const ItemPointer &position);

  virtual bool IsVisible(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tuple_id) = 0;

  bool IsVisbleOrDirty(__attribute__((unused)) const storage::Tuple *key,
                       const ItemPointer &position) {
    auto tile_group_header = catalog::Manager::GetInstance()
                                 .GetTileGroup(position.block)
                                 ->GetHeader();
    auto tuple_id = position.offset;

    txn_id_t tuple_txn_id = tile_group_header->GetTransactionId(tuple_id);
    cid_t tuple_begin_cid = tile_group_header->GetBeginCommitId(tuple_id);
    cid_t tuple_end_cid = tile_group_header->GetEndCommitId(tuple_id);
    if (tuple_txn_id == INVALID_TXN_ID) {
      // the tuple is not available.
      return false;
    }
    bool own = (current_txn->GetTransactionId() == tuple_txn_id);

    // there are exactly two versions that can be owned by a transaction.
    // unless it is an insertion.
    if (own == true) {
      if (tuple_begin_cid == MAX_CID && tuple_end_cid != INVALID_CID) {
        assert(tuple_end_cid == MAX_CID);
        // the only version that is visible is the newly inserted one.
        return true;
      } else {
        // the older version is not visible.
        return false;
      }
    } else {
      bool activated = (current_txn->GetBeginCommitId() >= tuple_begin_cid);
      bool invalidated = (current_txn->GetBeginCommitId() >= tuple_end_cid);
      if (tuple_txn_id != INITIAL_TXN_ID) {
        // if the tuple is owned by other transactions.
        if (tuple_begin_cid == MAX_CID) {
          // uncommitted version.
          if (tuple_end_cid == INVALID_CID) {
            // dirty delete is invisible
            return false;
          } else {
            // dirty update or insert is visible
            return true;
          }
        } else {
          // the older version may be visible.
          if (activated && !invalidated) {
            return true;
          } else {
            return false;
          }
        }
      } else {
        // if the tuple is not owned by any transaction.
        if (activated && !invalidated) {
          return true;
        } else {
          return false;
        }
      }
    }
  }

  virtual bool IsOwner(const storage::TileGroupHeader *const tile_group_header,
                       const oid_t &tuple_id) = 0;

  virtual bool IsOwnable(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tuple_id) = 0;

  virtual bool AcquireOwnership(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tile_group_id, const oid_t &tuple_id) = 0;

  virtual bool PerformInsert(const ItemPointer &location) = 0;

  virtual bool PerformRead(const ItemPointer &location) = 0;

  virtual void PerformUpdate(const ItemPointer &old_location,
                             const ItemPointer &new_location) = 0;

  virtual void PerformDelete(const ItemPointer &old_location,
                             const ItemPointer &new_location) = 0;

  virtual void PerformUpdate(const ItemPointer &location) = 0;

  virtual void PerformDelete(const ItemPointer &location) = 0;

  /*
   * Write a virtual function to push deleted and verified (acc to optimistic
   * concurrency control) tuples into possibly free from all underlying
   * concurrency implementations of transactions.
   */
  void RecycleTupleSlot(const oid_t &tile_group_id, const oid_t &tuple_id,
                        const cid_t &tuple_end_cid) {
    auto tile_group =
        catalog::Manager::GetInstance().GetTileGroup(tile_group_id);
    gc::GCManagerFactory::GetInstance().RecycleTupleSlot(
        tile_group->GetTableId(), tile_group_id, tuple_id, tuple_end_cid);
  }

  // Txn manager may store related information in TileGroupHeader, so when
  // TileGroup is dropped, txn manager might need to be notified
  virtual void DroppingTileGroup(const oid_t &tile_group_id
                                 __attribute__((unused))) {
    return;
  }

  void SetTransactionResult(const Result result) {
    current_txn->SetResult(result);
  }

  // for use by recovery
  void SetNextCid(cid_t cid) { next_cid_ = cid; }

  virtual Transaction *BeginTransaction() = 0;

  virtual void EndTransaction() = 0;

  virtual Result CommitTransaction() = 0;

  virtual Result AbortTransaction() = 0;

  void ResetStates() {
    next_txn_id_ = START_TXN_ID;
    next_cid_ = START_CID;
  }

  // this function generates the maximum commit id of committed transactions.
  // please note that this function only returns a "safe" value instead of a
  // precise value.
  virtual cid_t GetMaxCommittedCid() = 0;

  bool PerformEpochCAS(cid_t old_val, cid_t new_val) {
    bool retval = false;
    retval = cid_of_smallest_epoch_cleaned_.compare_exchange_strong(old_val,
                                                                    new_val);
    return retval;
  }

  oid_t GetCurrentEpochId() { return next_cid_.load(); }

  cid_t GetSmallestEpochCleanedCid() {
    return cid_of_smallest_epoch_cleaned_.load();
  }

  Epoch *GetEpoch(oid_t e) {
    Epoch *epoch = nullptr;
    if (epoch_map_.find(e, epoch)) {
      return epoch;
    }
    return nullptr;
  }

  void EraseEpoch(oid_t e) { epoch_map_.erase(e); }

  void AddEpochToMap(cid_t key, Epoch *e) { epoch_map_[key] = e; }

 private:
  std::atomic<txn_id_t> next_txn_id_;
  std::atomic<cid_t> next_cid_;
  std::atomic<oid_t> next_epoch_id_;
  std::atomic<cid_t> cid_of_smallest_epoch_cleaned_;
  cuckoohash_map<cid_t, Epoch *> epoch_map_;
};
}  // End storage namespace
}  // End peloton namespace
