#include "math/nnet/dense_layer.h"

#include <cassert>

namespace nnet {

DenseLayer::DenseLayer(
    const Dimensions& dimensions,
    const ActivationFunctionType& activation_function, size_t layer_index)
    : Super(dimensions, layer_index),
      generator_(dimensions),
      activation_function_(activation_function) {}

const std::vector<std::string>& DenseLayer::weights() const {
  return generator_.weights();
}

Matrix<symbolic::Expression> DenseLayer::GenerateExpression(
    const Matrix<symbolic::Expression>& input) const {
  auto dim = input.size();
  size_t rows = std::get<0>(dim);
  size_t cols = std::get<1>(dim);
  if ((rows != dimensions_.num_inputs) || (cols != 1)) {
    std::cerr << "Error: DenseLayer::GenerateExpression called on input "
                 "of incorrect size: "
              << "(" << rows << ", " << cols << ")" << std::endl;
    std::exit(1);
  }
  // +1 in number of columns to account for added bias weights.
  Matrix<symbolic::Expression> weight_matrix(dimensions_.num_outputs,
                                             dimensions_.num_inputs + 1);
  for (size_t i = 0; i < dimensions_.num_outputs; ++i) {
    for (size_t j = 0; j < dimensions_.num_inputs; ++j) {
      weight_matrix.at(i, j) = symbolic::CreateExpression(generator_.W(i, j));
    }
  }

  // Add layer bias.
  auto biased_input = AddBias(input);
  for (size_t i = 0; i < dimensions_.num_outputs; ++i) {
    // Bias is the final column in the weights matrix. Since size
    // is kNumInputs + 1 and it is zero-indexed, kNumInputs is the final
    // index (index of bias).
    weight_matrix.at(i, dimensions_.num_inputs) =
        symbolic::CreateExpression(generator_.W(i));
  }

  return (weight_matrix * biased_input).Map(activation_function_);
}

std::unique_ptr<LayerImpl> DenseLayer::Clone() const {
  return std::make_unique<DenseLayer>(dimensions_, activation_function_,
                                            layer_index_);
}

}  // namespace nnet