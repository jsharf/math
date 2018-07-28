#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "math/symbolic/expression_node.h"
#include "math/symbolic/numeric_value.h"

#include <experimental/optional>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace symbolic {

// Used to map the environment in which an expression is evaluated (binds
// variable names to their values).
//
// For instance, x^2 + y evaluated in the environment { x: "2", y: "3"} becomes
// 2^2 + 3, or 7.
using Environment = std::unordered_map<std::string, symbolic::NumericValue>;

class AdditionExpression;
class Expression;

Expression CreateExpression(std::string expression);

// Class which holds the ExpressionNode tree and provides an easy-to-use
// interface.
class Expression {
 public:
  Expression() : Expression(0) {}

  Expression(std::shared_ptr<const ExpressionNode> root);

  Expression(const Expression& other);

  Expression(Expression&& rhs);

  Expression(const NumericValue& value);

  Expression(Number a);

  Expression operator+(const Expression& rhs) const;
  Expression operator-(const Expression& rhs) const;
  Expression operator*(const Expression& rhs) const;
  Expression operator/(const Expression& rhs) const;

  // Returns log of exp with base base. Aka log(exp)/log(base).
  static Expression Log(NumericValue base, const Expression& exp);

  // Returns base^expression.
  static Expression Exp(NumericValue base, const Expression& exp);

  // TODO(sharf): make this immutable, remove these.
  Expression& operator=(const Expression& rhs);
  Expression& operator=(Expression&& rhs);

  // TODO(sharf): make this immutable, remove this.
  Expression& operator+=(const Expression& rhs);

  friend std::ostream& operator<<(std::ostream& output, const Expression& exp) {
    output << exp.to_string();
    return output;
  }

  // Variables which need to be resolved in order to evaluate the expression.
  std::set<std::string> variables() const;

  Expression Bind(const std::string& name, NumericValue value) const;

  Expression Bind(const Environment& env) const;

  Expression Derive(const std::string& x) const;

  std::experimental::optional<NumericValue> Evaluate() const;

  // TODO(sharf): make this immutable, remove this.
  void Reset(std::shared_ptr<const ExpressionNode> root);

  const std::shared_ptr<const ExpressionNode>& GetPointer() const {
    return expression_root_;
  }

  std::string to_string() const;

 private:
  std::shared_ptr<const ExpressionNode> expression_root_;
};

class IfExpression : public ExpressionNode {
 public:
  // if(conditional) { a } else { b }
  IfExpression(const Expression& conditional, const Expression& a,
               const Expression& b)
      : conditional_(conditional), a_(a), b_(b) {}

  // Variables which need to be resolved in order to evaluate the expression.
  std::set<std::string> variables() const override;

  // Bind variables to values to create an expression which can be evaluated.
  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // If all variables in the expression have been bound, this produces a
  // numerical evaluation of the expression.
  std::experimental::optional<NumericValue> TryEvaluate() const override;

  // Returns the symbolic partial derivative of this expression.
  // Note: This does not do any bounds analysis and simply returns
  // IfExpression(conditional_, a_->Derive(x), b_->Derive(x)).
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;

  // Converts IfExpression to string form:
  // (( conditional ) ? ( a ) : ( b ))
  std::string to_string() const override;

  std::shared_ptr<const ExpressionNode> Clone() const override {
    std::shared_ptr<IfExpression> clone =
        std::make_shared<IfExpression>(conditional_, a_, b_);
    return clone;
  }

 private:
  Expression conditional_;
  Expression a_;
  Expression b_;
};

class CompoundExpression : public ExpressionNode {
 public:
  std::set<std::string> variables() const override;

  virtual std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override = 0;

  std::experimental::optional<NumericValue> TryEvaluate() const override;

  std::string to_string() const override;

  virtual NumericValue reduce(const NumericValue& a,
                              const NumericValue& b) const = 0;

  virtual std::string operator_to_string() const = 0;

  virtual std::shared_ptr<const ExpressionNode> Clone() const override = 0;

 protected:
  CompoundExpression(const Expression& a, const Expression& b)
      : head_(a), tail_(b), is_end_(false) {}
  CompoundExpression(const Expression& head) : head_(head), is_end_(true) {}
  Expression head_;
  Expression tail_;
  bool is_end_;
};

class AdditionExpression : public CompoundExpression {
 public:
  AdditionExpression(const Expression& a, const Expression& b)
      : CompoundExpression(a, b) {}
  AdditionExpression(const Expression& a) : CompoundExpression(a) {}
  NumericValue reduce(const NumericValue& a,
                      const NumericValue& b) const override;
  std::string operator_to_string() const override { return "+"; }

  std::shared_ptr<const ExpressionNode> Clone() const override {
    if (is_end_) {
      return std::static_pointer_cast<const ExpressionNode>(
          std::make_shared<AdditionExpression>(head_));
    }
    return std::static_pointer_cast<const ExpressionNode>(
        std::make_shared<AdditionExpression>(head_, tail_));
  }

  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // Returns the symbolic partial derivative of this expression.
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;
};

class MultiplicationExpression : public CompoundExpression {
 public:
  MultiplicationExpression(const Expression& a, const Expression& b)
      : CompoundExpression(a, b) {}

  MultiplicationExpression(const Expression& a) : CompoundExpression(a) {}

  NumericValue reduce(const NumericValue& a,
                      const NumericValue& b) const override;

  std::string operator_to_string() const override { return "*"; }

  std::shared_ptr<const ExpressionNode> Clone() const override {
    if (is_end_) {
      return std::static_pointer_cast<const ExpressionNode>(
          std::make_shared<MultiplicationExpression>(head_));
    }
    return std::static_pointer_cast<const ExpressionNode>(
        std::make_shared<MultiplicationExpression>(head_, tail_));
  }

  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // Returns the symbolic partial derivative of this expression.
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;
};

// Boolean &&
class AndExpression : public CompoundExpression {
 public:
  AndExpression(const Expression& a, const Expression& b)
      : CompoundExpression(a, b) {}

  AndExpression(const Expression& a) : CompoundExpression(a) {}

  NumericValue reduce(const NumericValue& a,
                      const NumericValue& b) const override;

  std::string operator_to_string() const override { return "&&"; }

  std::shared_ptr<const ExpressionNode> Clone() const override {
    if (is_end_) {
      return std::static_pointer_cast<const ExpressionNode>(
          std::make_shared<AndExpression>(head_));
    }
    return std::static_pointer_cast<const ExpressionNode>(
        std::make_shared<AndExpression>(head_, tail_));
  }

  static bool ToBool(Number x) {
    return abs(x) > std::numeric_limits<Number>::epsilon();
  }

  static Number ToNumber(bool b) { return b ? 1.0 : 0.0; }

  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // Returns the symbolic partial derivative of this expression.
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;
};

// >= Conditional expression

class GteExpression : public ExpressionNode {
 public:
  GteExpression(const Expression& a, const Expression& b) : a_(a), b_(b) {}

  // Variables which need to be resolved in order to evaluate the expression.
  std::set<std::string> variables() const override;

  // Bind variables to values to create an expression which can be evaluated.
  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // If all variables in the expression have been bound, this produces a
  // numerical evaluation of the expression.
  std::experimental::optional<NumericValue> TryEvaluate() const override;

  // Not defined for GteExpression. Returns nullptr.
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;

  std::string to_string() const override;

  std::shared_ptr<const ExpressionNode> Clone() const override {
    std::shared_ptr<GteExpression> clone =
        std::make_shared<GteExpression>(a_, b_);
    return clone;
  }

 private:
  Expression a_;
  Expression b_;
};

class DivisionExpression : public ExpressionNode {
 public:
  DivisionExpression(const Expression& numerator, const Expression& denominator)
      : numerator_(numerator), denominator_(denominator) {}

  DivisionExpression() {}

  // Variables which need to be resolved in order to evaluate the expression.
  std::set<std::string> variables() const override;

  // Bind variables to values to create an expression which can be evaluated.
  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // If all variables in the expression have been bound, this produces a
  // numerical evaluation of the expression.
  std::experimental::optional<NumericValue> TryEvaluate() const override;

  // Returns the symbolic partial derivative of this expression.
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;

  std::string to_string() const override;

  std::shared_ptr<const ExpressionNode> Clone() const override {
    std::shared_ptr<DivisionExpression> clone =
        std::make_shared<DivisionExpression>(numerator_, denominator_);
    return std::move(clone);
  }

 private:
  Expression numerator_;
  Expression denominator_;
};

// b^x.
class ExponentExpression : public ExpressionNode {
 public:
  ExponentExpression(const NumericValue& b, const Expression& child)
      : b_(b), child_(child) {}

  // Variables which need to be resolved in order to evaluate the expression.
  std::set<std::string> variables() const override {
    return child_.variables();
  }

  // Bind variables to values to create an expression which can be evaluated.
  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // If all variables in the expression have been bound, this produces a
  // numerical evaluation of the expression.
  std::experimental::optional<NumericValue> TryEvaluate() const override;

  // Returns the symbolic partial derivative of this expression.
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;

  std::string to_string() const override;

  std::shared_ptr<const ExpressionNode> Clone() const override;

 private:
  NumericValue b_;
  Expression child_;
};

// NOTE: as a judgement call, I'm not implementing LogExpression with complex
// values. If you'd like to implement that, here's the details:
// https://en.wikipedia.org/wiki/Complex_logarithm
class LogExpression : public ExpressionNode {
 public:
  LogExpression(const NumericValue& base, const Expression& child)
      : b_(base), child_(child) {}

  // Variables which need to be resolved in order to evaluate the expression.
  std::set<std::string> variables() const override {
    return child_.variables();
  }

  // Bind variables to values to create an expression which can be evaluated.
  std::shared_ptr<const ExpressionNode> Bind(
      const Environment& env) const override;

  // If all variables in the expression have been bound, this produces a
  // numerical evaluation of the expression.
  std::experimental::optional<NumericValue> TryEvaluate() const override;

  // Returns the symbolic partial derivative of this expression.
  std::shared_ptr<const ExpressionNode> Derive(
      const std::string& x) const override;

  std::string to_string() const override;

  std::shared_ptr<const ExpressionNode> Clone() const override;

 private:
  NumericValue b_;
  Expression child_;
};

}  // namespace symbolic

#endif /* EXPRESSION_H */
