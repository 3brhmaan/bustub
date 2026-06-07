#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/hash_join_plan.h"
#include "execution/plans/nested_loop_join_plan.h"
#include "execution/plans/projection_plan.h"
#include "fmt/core.h"
#include "optimizer/optimizer.h"
#include "type/type_id.h"

namespace bustub {

void BuiltLeftAndRightKeyExpression(const AbstractExpressionRef &expression,
                                    std::vector<AbstractExpressionRef> &left_key_expressions,
                                    std::vector<AbstractExpressionRef> &right_key_expressions) {
  if (expression->GetChildren().empty()) {
    auto column_value_expression = dynamic_cast<const ColumnValueExpression &>(*expression);

    if (column_value_expression.GetTupleIdx() == 0) {
      left_key_expressions.emplace_back(expression);
    } else {
      //* the tuple index will be "1" and i want it to be "0" to be like this: " HashJoin { type=Inner, left_key=[#0.0,
      //* #0.1], right_key=[#0.0, #0.2] } "
      auto updated_expression = std::make_shared<ColumnValueExpression>(0, column_value_expression.GetColIdx(),
                                                                        column_value_expression.GetReturnType());
      right_key_expressions.emplace_back(updated_expression);
    }
  } else {
    for (const auto &child : expression->GetChildren()) {
      BuiltLeftAndRightKeyExpression(child, left_key_expressions, right_key_expressions);
    }
  }
}

auto Optimizer::OptimizeNLJAsHashJoin(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  std::vector<AbstractPlanNodeRef> children;
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeNLJAsHashJoin(child));
  }

  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  if (optimized_plan->GetType() == PlanType::NestedLoopJoin) {
    const auto &nlj_plan = dynamic_cast<const NestedLoopJoinPlanNode &>(*optimized_plan);
    auto predicate = nlj_plan.Predicate();

    if (predicate->GetChildren().empty()) {
      return optimized_plan;
    }

    std::vector<AbstractExpressionRef> left_key_expressions{};
    std::vector<AbstractExpressionRef> right_key_expressions{};
    BuiltLeftAndRightKeyExpression(predicate, left_key_expressions, right_key_expressions);

    return std::make_shared<HashJoinPlanNode>(nlj_plan.output_schema_, nlj_plan.GetLeftPlan(), nlj_plan.GetRightPlan(),
                                              left_key_expressions, right_key_expressions, nlj_plan.GetJoinType());
  }

  return optimized_plan;
}

}  // namespace bustub
