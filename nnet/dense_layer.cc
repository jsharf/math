#include "math/nnet/dense_layer.h"

#include <cassert>

namespace nnet {

DenseLayer::DenseLayer(const Dimensions& dimensions,
                       const ActivationFunctionType& activation_function,
                       size_t layer_index)
    : Super(dimensions, layer_index),
      generator_(dimensions),
      activation_function_(activation_function) {}

const std::vector<std::string>& DenseLayer::weights() const {
  return generator_.weights();
}

symbolic::Expression DenseLayer::GenerateOutputCode(
    const symbolic::Expression& output_index) const {
  symbolic::Expression sum = 0;
  for (size_t i = 0; i < dimensions_.num_inputs; ++i) {
    sum += generator_.BoundsCheckedW(output_index, Expression(i)) * generator_.I(i);
  }

  // Bias input.
  sum += generator_.W(output_index) * 1;

  return activation_function_(sum);
}

// The input gradient is just the sum of the weights between each output and that
// input multiplied by the back-propagated gradients.
symbolic::Expression DenseLayer::InputGradientCode(
    const symbolic::Expression& input_index) const {
  symbolic::Expression sum = 0;
  for (size_t out_index = 0; out_index < dimensions_.num_outputs; ++out_index) {
    sum += generator_.GRADIENT(out_index) * generator_.BoundsCheckedW(symbolic::Expression::CreateInteger(out_index), input_index);
  }
  return sum;
}

// The weight gradient is just the input for that weight multiplied by the
// back-propagated gradient.
symbolic::Expression DenseLayer::WeightGradientCode(
    const symbolic::Expression& weight_index) const {
  // Unflatten the weight index to node, edge.
  symbolic::Expression node = Unflatten2dRow(dimensions_.num_inputs + 1, dimensions_.num_outputs, weight_index);
  symbolic::Expression edge = Unflatten2dCol(dimensions_.num_inputs + 1, dimensions_.num_outputs, weight_index);
  return generator_.GRADIENT(node) * generator_.I(edge);
}

std::unique_ptr<LayerImpl> DenseLayer::Clone() const {
  return std::make_unique<DenseLayer>(dimensions_, activation_function_,
                                      layer_index_);
}

}  // namespace nnet
