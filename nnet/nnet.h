#ifndef NNET_H
#define NNET_H
#include "math/geometry/matrix.h"
#include "math/nnet/layer.h"
#include "math/stats/normal.h"
#include "math/symbolic/expression.h"
#include "math/symbolic/symbolic_util.h"

#include <map>

namespace nnet {

typedef double Number;

// TODO(sharf): Things like I(), W(), weights, and evaluators should be
// encapsulated in some sort of interface.

// Creates a neural network symbolically. The input layer has no weights. There
// are kNumHiddenLayers + 2 layers with weights (+1 for output layer, +1 for
// first layer which has kInputSize number of weights per neuron).
// TODO(sharf): Implement code generation -- an expression can render itself
// into glsl for parallel execution. That will make this useful.
template <size_t kNumHiddenLayers, size_t kLayerSize, size_t kOutputSize,
          size_t kInputSize>
class Nnet {
 public:
  // + 1 for bias.
  static constexpr size_t kNumLayers = kNumHiddenLayers + 2;

  using SymbolicInputVector = Matrix<kInputSize, 1, symbolic::Expression>;
  using SymbolicOutputVector = Matrix<kOutputSize, 1, symbolic::Expression>;
  using InputVector = Matrix<kInputSize, 1, Number>;
  using OutputVector = Matrix<kOutputSize, 1, Number>;
  using HiddenVector = Matrix<kLayerSize, 1, symbolic::Expression>;

  struct LearningParameters {
    Number learning_rate;
    bool dynamic_learning_rate = false;
  };

  Nnet() {
    SymbolicInputVector inputs = GenInputLayer();
    FeedForwardLayer<kInputSize, kLayerSize> input_layer_generator(&generator_,
                                                                   0);

    HiddenVector layer = input_layer_generator.GenerateExpression(inputs);

    for (size_t layer_idx = 1; layer_idx <= kNumHiddenLayers; ++layer_idx) {
      FeedForwardLayer<kLayerSize, kLayerSize> hidden_layer_generator(
          &generator_, layer_idx);
      layer = hidden_layer_generator.GenerateExpression(layer);
    }

    FeedForwardLayer<kLayerSize, kOutputSize> output_layer_generator(
        &generator_, kNumLayers - 1);
    neural_network_ = output_layer_generator.GenerateExpression(layer);

    CalculateInitialWeights();
  }

  // This class generates symbol names for neural network values. Since these
  // will be used for codegen for opencl, the symbols are all one-dimensional
  // indices into arrays.
  class FlatWeightSymbolGenerator : public SymbolGenerator {
   public:
    virtual std::string W(size_t layer, size_t node, size_t edge) {
      auto tuple = std::make_tuple(layer, node, edge);
      if (weight_index_.count(tuple) == 0) {
        weight_index_[tuple] = weight_count_;
        rev_weight_index_[weight_count_] = tuple;
        weight_count_++;
      }
      return "W[" + std::to_string(weight_index_[tuple]) + "]";
    }
    virtual std::string I(size_t i) const {
      return "I(" + std::to_string(i) + ")";
    }
    virtual std::string O(size_t i) const {
      return "O[" + std::to_string(i) + "]";
    }

    // Used to interpret results from opencl call.
    std::map<int, std::tuple<int, int, int>> reverse_weight_map() const {
      return rev_weight_index_;
    }

   private:
    // Mapping from <layer, node, edge> -> int. This lets each weight have a
    // single unique index.
    std::map<std::tuple<int, int, int>, int> weight_index_;
    // Reverse mapping.
    std::map<int, std::tuple<int, int, int>> rev_weight_index_;
    size_t weight_count_ = 0;
  };

  OutputVector Evaluate(InputVector in) const {
    // Modify weights_ in-place to avoid copying them. This is guaranteed to
    // never have stale inputs (from previous eval) as long as I follow naming
    // conventions and don't fuck up (I probably will).
    symbolic::Environment weights = weights_;
    for (size_t i = 0; i < kInputSize; ++i) {
      weights[generator_.I(i)].real() = in.at(i, 0);
    }

    std::function<symbolic::Expression(const symbolic::Expression&)> binder =
        [&weights](const symbolic::Expression& exp) {
          return exp.Bind(weights);
        };

    Matrix<kOutputSize, 1, symbolic::Expression> bound_network =
        neural_network_.Map(binder);

    std::function<Number(const symbolic::Expression&)> real_evaluator =
        [](const symbolic::Expression& exp) {
          auto maybe_value = exp.Evaluate();
          if (!maybe_value) {
            // Shit.
            std::cerr << "Well, fuck, not sure how this happened" << std::endl;
          }
          return maybe_value->real();
        };

    Matrix<kOutputSize, 1, Number> results = bound_network.Map(real_evaluator);

    return results;
  }

  // Back propagation
  void Train(InputVector in, OutputVector o, const LearningParameters& params) {
    for (size_t i = 0; i < kInputSize; ++i) {
      weights_[generator_.I(i)].real() = in.at(i, 0);
    }
    for (size_t i = 0; i < kOutputSize; ++i) {
      weights_[generator_.O(i)].real() = o.at(i, 0);
    }

    symbolic::Expression error = GetErrorExpression();
    symbolic::Environment weights = weights_;

    Number learning_rate = params.learning_rate;

#pragma omp parallel for shared(error) shared(weights)
    for (const auto& kv : weights) {
      const std::string& weight_name = kv.first;
      symbolic::NumericValue value = kv.second;
      symbolic::Expression symbolic_gradient = error.Derive(weight_name);
      symbolic::Expression gradient = symbolic_gradient.Bind(weights);
      auto gradient_value = gradient.Evaluate();
      if (!gradient_value) {
        std::cerr << "Shit" << std::endl;
        for (const std::string& variable : gradient.variables()) {
          std::cerr << variable << std::endl;
        }
        std::cerr << WeightsToString() << std::endl;
      }
      Number weight_update = -gradient_value->real() * learning_rate;
#pragma omp atomic
      weights_[weight_name].real() += weight_update;
    }
  }

  symbolic::Expression GetErrorExpression() const {
    symbolic::Expression error;
    for (size_t out_idx = 0; out_idx < kOutputSize; ++out_idx) {
      symbolic::Expression output_error =
          neural_network_.at(out_idx, 0) -
          symbolic::Expression(generator_.O(out_idx));
      error = error + (output_error * output_error);
    }
    return error;
  }

  SymbolicOutputVector GetExpression() const { return neural_network_; }

  std::string to_string() const { return neural_network_.to_string(); }

  std::string WeightsToString() const {
    std::stringstream output;
    output << "{";
    for (const auto& kv : weights_) {
      output << kv.first << ":" << kv.second.to_string() << "," << std::endl;
    }
    output << "}";
    return output.str();
  }

 private:
  void CalculateInitialWeights() {
    FeedForwardLayer<kInputSize, kLayerSize> input_layer_generator(&generator_,
                                                                   0);
    stats::Normal I = input_layer_generator.XavierInitializer();
    for (const std::string& weight : input_layer_generator.weights()) {
      weights_[weight].real() = I.sample();
    }
    for (size_t layer = 1; layer <= kNumHiddenLayers; ++layer) {
      FeedForwardLayer<kLayerSize, kLayerSize> hidden_layer_generator(
          &generator_, layer);
      stats::Normal H = hidden_layer_generator.XavierInitializer();
      for (const std::string& weight : hidden_layer_generator.weights()) {
        weights_[weight].real() = H.sample();
      }
    }
    FeedForwardLayer<kLayerSize, kOutputSize> output_layer_generator(
        &generator_, kNumLayers - 1);
    stats::Normal O = output_layer_generator.XavierInitializer();
    for (const std::string& weight : output_layer_generator.weights()) {
      weights_[weight].real() = O.sample();
    }
  }

  SymbolicInputVector GenInputLayer() const {
    SymbolicInputVector result;
    for (size_t i = 0; i < kInputSize; ++i) {
      result.at(i, 0) = symbolic::CreateExpression(generator_.I(i));
    }
    return result;
  }

  // The entire neural network is stored symbolically, in a column vector of
  // type symbolic::Expression. To get your outputs, simply call Bind() on all
  // expressions in the column vector and bind weights and inputs.
  //
  // Weight names are of the form:
  // W(i,j,k) -- i is layer number, j is the index of the neuron in that
  // layer.
  // k Is the index of the neuron in the previous layer that's connected to
  // this
  // one (or for the input layer's case, the index of the input).
  // I(i) -- i is the index of the input.
  //
  // All indices and layer numbers are zero-indexed.
  //
  // The first layer has kInputSize nuerons. Every Hidden layer has kLayerSize
  // neurons. The output layer has kOutputSize neurons. There are
  // kNumHiddenLayers + 1 layers with weights (the hidden layers and the
  // output
  // layer).
  Matrix<kOutputSize, 1, symbolic::Expression> neural_network_;

  FlatWeightSymbolGenerator generator_;

  symbolic::Environment weights_;

  // For efficiently iterating through weights.
  std::vector<std::string> weight_list_;
};

}  // namespace nnet

#endif /* NNET_H */
