#ifndef LAYER_IMPL_H
#define LAYER_IMPL_H

#include "codegen/codegen.h"
#include "geometry/dynamic_matrix.h"
#include "nnet/layer_dimensions.h"
#include "stats/normal.h"
#include "symbolic/expression.h"
#include "symbolic/symbolic_util.h"

namespace nnet {

// Adds a bias input to the end of a column vector.
Matrix<symbolic::Expression> AddBias(Matrix<symbolic::Expression> x);

class LayerImpl {
 public:
  // Dim(num_outputs * (num_inputs + 1))
  // TODO(sharf): use std::vector<std::string> instead and rename weights() to
  // weightnames();
  using ActivationFunctionType =
      std::function<symbolic::Expression(const symbolic::Expression&)>;

  virtual const std::vector<std::string>& weights() const {
    // Default implementation.
    static const std::vector<std::string> empty = std::vector<std::string>();
    return empty;
  }

  virtual void GenerateOutputCode(const symbolic::Expression &output_index,
                                  codegen::Generator *cg) const = 0;

  virtual void InputGradientCode(const symbolic::Expression &input_index,
                                 codegen::Generator *cg) const = 0;

  virtual void WeightGradientCode(const symbolic::Expression &weight_index,
                                  codegen::Generator *cg) const = 0;

  virtual std::unique_ptr<LayerImpl> Clone() const = 0;

  Dimensions GetDimensions() const { return dimensions_; }

  size_t layer_index() const { return layer_index_; }

  virtual std::string layer_type() const = 0;

  virtual ~LayerImpl() {}

 protected:
  LayerImpl(const Dimensions& dimensions, size_t layer_index)
      : dimensions_(dimensions), layer_index_(layer_index) {}

  Dimensions dimensions_;
  size_t layer_index_;
};

}  // namespace nnet

#endif /* LAYER_IMPL_H */
