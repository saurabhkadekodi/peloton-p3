/**
 * @brief Header file for limit executor.
 *
 * Copyright(c) 2015, CMU
 */

#pragma once

#include "backend/common/types.h"
#include "backend/executor/abstract_executor.h"

namespace peloton {
namespace executor {

/**
 * TODO Currently, both limit and offset must be good numbers.
 * Postgres also allows stand-alone LIMIT and stand-alone OFFSET.
 * Need further change to accommodate it.
 */
class LimitExecutor : public AbstractExecutor {
 public:
  LimitExecutor(const LimitExecutor &) = delete;
  LimitExecutor &operator=(const LimitExecutor &) = delete;
  LimitExecutor(const LimitExecutor &&) = delete;
  LimitExecutor &operator=(const LimitExecutor &&) = delete;

  explicit LimitExecutor(planner::AbstractPlanNode *node,
                         ExecutorContext *executor_context);

 protected:
  bool DInit();

  bool DExecute();

 private:
  //===--------------------------------------------------------------------===//
  // Executor State
  //===--------------------------------------------------------------------===//

  /** @brief Number of tuples skipped. */
  size_t num_skipped_ = 0;

  /** @brief Number of tuples returned. */
  size_t num_returned_ = 0;
};

} /* namespace executor */
} /* namespace peloton */
