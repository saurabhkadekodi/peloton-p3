//===----------------------------------------------------------------------===//
//
//                         PelotonDB
//
// catalog_test.cpp
//
// Identification: tests/catalog/constraints_test.cpp
//
// Copyright (c) 2016, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"
#include "harness.h"

#include "backend/catalog/schema.h"
#include "backend/common/value.h"
#include "backend/concurrency/transaction.h"
#include "backend/concurrency/transaction_manager_factory.h"
#include "backend/executor/abstract_executor.h"
#include "backend/storage/tile_group_factory.h"
#include "backend/storage/tuple.h"
#include "backend/storage/table_factory.h"
#include "backend/index/index_factory.h"

#include "catalog/constraints_tests_util.h"


namespace peloton {
namespace test {

//===--------------------------------------------------------------------===//
// Constraints Tests
//===--------------------------------------------------------------------===//

class ConstraintsTests : public PelotonTest {};

TEST_F(ConstraintsTests, NOTNULLTest) {
  // First, generate the table with index
  std::unique_ptr<storage::DataTable> data_table(
      ConstraintsTestsUtil::CreateAndPopulateTable());

  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();

  // begin this transaction
  txn_manager.BeginTransaction();

  const catalog::Schema *schema = data_table->GetSchema();


  // Test1: insert a tuple with column 1 = null
  storage::Tuple tuple1(schema, true);
  auto testing_pool = TestingHarness::GetInstance().GetTestingPool();

  tuple1.SetValue(0, ValueFactory::GetNullValue(),
                  testing_pool);
  tuple1.SetValue(
      1, ValueFactory::GetIntegerValue(ConstraintsTestsUtil::PopulatedValue(1, 1)),
      testing_pool);
  tuple1.SetValue(
      2, ValueFactory::GetIntegerValue(ConstraintsTestsUtil::PopulatedValue(1, 2)),
      testing_pool);
  Value string_value =
      ValueFactory::GetStringValue(std::to_string(ConstraintsTestsUtil::PopulatedValue(
          1, 3)));
  tuple1.SetValue(3, string_value, testing_pool);

  bool hasException = false;
  try {
    data_table->InsertTuple(&tuple1);
  } catch (ConstraintException e){
    hasException = true;
  }
  EXPECT_TRUE(hasException);


  // Test2: insert a legal tuple
  storage::Tuple tuple2(schema, true);

  tuple2.SetValue(0, ValueFactory::GetIntegerValue(ConstraintsTestsUtil::PopulatedValue(0, 1)),
                  testing_pool);
  tuple2.SetValue(
      1, ValueFactory::GetIntegerValue(ConstraintsTestsUtil::PopulatedValue(1, 1)),
      testing_pool);
  tuple2.SetValue(
      2, ValueFactory::GetIntegerValue(ConstraintsTestsUtil::PopulatedValue(1, 2)),
      testing_pool);
  tuple2.SetValue(3, string_value, testing_pool);

  hasException = false;
  try {
    data_table->InsertTuple(&tuple2);
  } catch (ConstraintException e){
    hasException = true;
  }
  EXPECT_TRUE(!hasException);

  // commit this transaction
  txn_manager.CommitTransaction();

}

}  // End test namespace
}  // End peloton namespace
