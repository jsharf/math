#include <sstream>
#include "symbolic/numeric_value.h"

namespace symbolic {

std::shared_ptr<const ExpressionNode> NumericValue::Bind(
    const std::unordered_map<std::string, std::unique_ptr<NumericValue>>& env)
    const {
  if (!is_bound_) {
    if (env.count(name_) == 1) {
      std::unique_ptr<NumericValue> value = env.at(name_)->CloneValue();
      return std::shared_ptr<NumericValue>(value.release());
    }
  }
  return std::shared_ptr<const ExpressionNode>(Clone().release());
}

std::set<std::string> NumericValue::variables() const {
  if (is_bound_) {
    return std::set<std::string>{};
  }
  return std::set<std::string>{name_};
}

std::unique_ptr<NumericValue> NumericValue::TryEvaluate() const {
  if (!is_bound_) {
    return nullptr;
  }
  return std::make_unique<NumericValue>(*this);
}

// Returns the symbolic partial derivative of this expression.
std::shared_ptr<const ExpressionNode> NumericValue::Derive(
    const std::string& x) const {
  if (!is_bound_ && (name_ == x)) {
    return std::make_shared<NumericValue>(1.0);
  }
  return std::make_shared<NumericValue>(0.0);
}

std::string NumericValue::to_string() const {
  if (!is_bound_) {
    return name_;
  }
  std::ostringstream result;
  result << std::showpoint;
  result << a_;
  if (b_ != 0) {
    result << " + " << b_ << "i";
  }
  return result.str();
}

std::unique_ptr<const ExpressionNode> NumericValue::Clone() const {
  if (is_bound_) {
    return std::make_unique<NumericValue>(a_, b_);
  } else {
    return std::make_unique<NumericValue>(name_);
  }
}

std::unique_ptr<NumericValue> NumericValue::CloneValue() const {
  if (is_bound_) {
    return std::make_unique<NumericValue>(a_, b_);
  } else {
    return std::make_unique<NumericValue>(name_);
  }
}

const NumericValue NumericValue::pi(3.141592653589793238);
const NumericValue NumericValue::e(2.718281828459045235);

}  // namespace symbolic
