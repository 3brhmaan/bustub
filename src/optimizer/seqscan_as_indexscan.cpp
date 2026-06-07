#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/logger.h"
#include "common/macros.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/index_scan_plan.h"
#include "execution/plans/seq_scan_plan.h"
#include "fmt/core.h"
#include "optimizer/optimizer.h"

namespace bustub {

auto SplitStringByDot(const std::string &str) -> std::pair<std::string, std::string> {
  size_t pos = str.find('.');
  if (pos == std::string::npos) {
    //* it's const value like (i.e. 1)
    return {"<val>", "<val>"};
  }
  // substr(pos + 1) safely returns an empty string if '.' is at the end
  return std::make_pair(str.substr(0, pos), str.substr(pos + 1));
}

void SplitFilterPredicate(const AbstractExpressionRef &expression,
                          std::vector<std::pair<std::string, std::string>> &table_column_names,
                          std::vector<AbstractExpressionRef> &pred_keys, std::map<std::string, bool> &column_exist) {
  //* no other children then split into table name and column name and constant value
  if (expression->GetChildren().empty()) {
    // LOG_DEBUG("Expression: %s | Return Type: %s", expression->ToString().c_str(),

    //* get the table name and column name of the predicate (i.e. where test_seq_scan.v1 = 1)
    auto [table_name, column_name] = SplitStringByDot(expression->GetReturnType().GetName());

    //* it's a const value (i.e. 1)
    if (table_name == "<val>") {
      auto fake_tuple = Tuple{RID{-1}};
      auto fake_schema = Schema{std::vector<Column>{}};
      auto constant_value_expression =
          std::make_shared<ConstantValueExpression>(expression->Evaluate(&fake_tuple, fake_schema));

      // LOG_DEBUG("Inserting: %s", constant_value_expression->ToString().c_str());
      pred_keys.emplace_back(constant_value_expression);
      // fmt::println("");
      return;
    }

    // LOG_DEBUG("Table Name: %s, Column Name: %s\n", table_name.c_str(), column_name.c_str());
    if (column_exist.find(column_name) == column_exist.end()) {
      table_column_names.emplace_back(std::make_pair(table_name, column_name));
      column_exist[column_name] = true;
    }

  } else {
    for (const auto &child : expression->GetChildren()) {
      SplitFilterPredicate(child, table_column_names, pred_keys, column_exist);
    }
  }
}

auto Optimizer::OptimizeSeqScanAsIndexScan(const bustub::AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  std::vector<AbstractPlanNodeRef> children;
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeSeqScanAsIndexScan(child));
  }

  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  if (optimized_plan->GetType() == PlanType::SeqScan) {
    const auto &seqscan_plan = dynamic_cast<const SeqScanPlanNode &>(*optimized_plan);

    BUSTUB_ASSERT(optimized_plan->children_.empty(), "must have no children");  //* it have no children

    //* if it doesn't have any filter then no need to optimize it and seq scan is right
    if (seqscan_plan.filter_predicate_ == nullptr) {
      return optimized_plan;
    }

    std::vector<std::pair<std::string, std::string>> table_column_names;
    std::vector<AbstractExpressionRef> pred_keys;
    std::map<std::string, bool> column_exist;
    SplitFilterPredicate(seqscan_plan.filter_predicate_, table_column_names, pred_keys, column_exist);
    // LOG_DEBUG("Predicate: %s\n", seqscan_plan.filter_predicate_->ToString().c_str());
    // LOG_DEBUG("Pred_Keys Size: %zu", pred_keys.size());

    //? Note that queries of the form SELECT * FROM t1 WHERE v1 = 1 AND v2 = 2 should still use a seq scan
    if (table_column_names.size() == 1) {
      auto [table_name, column_name] = table_column_names[0];

      //* check if there is an index on the same column as the predicate then create the index_scan_plan_node
      auto indexes_info = catalog_.GetTableIndexes(table_name);
      for (auto &index_info : indexes_info) {
        BUSTUB_ASSERT(index_info->key_schema_.GetColumnCount() == 1, "Columns on index is more than one");
        auto index_coulmn_name = index_info->key_schema_.GetColumn(0).GetName();

        if (index_coulmn_name == column_name) {
          return std::make_shared<IndexScanPlanNode>(seqscan_plan.output_schema_, seqscan_plan.table_oid_,
                                                     index_info->index_oid_, seqscan_plan.filter_predicate_, pred_keys);
        }
      }
    }
  }

  return optimized_plan;
}

}  // namespace bustub
