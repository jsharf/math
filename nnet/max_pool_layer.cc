#include "math/nnet/max_pool_layer.h"
#include "math/symbolic/symbolic_util.h"

namespace nnet {

MaxPoolLayer::MaxPoolLayer(const VolumeDimensions& input,
                           const AreaDimensions& output, size_t layer_index)
    : Super(MaxPoolLayer::GenLinearDimensions(input, output), layer_index),
      generator_(input),
      input_(input),
      target_(output) {
  if (dimensions_.num_outputs > dimensions_.num_inputs) {
    std::cerr << "Error: MaxPoolLayer " << layer_index
              << " constructed with more outputs than inputs?" << std::endl
              << "3D inputs: " << input.width << ", " << input.height << ", "
              << input.depth << std::endl
              << "3D outputs: " << output.width << ", " << output.height << ", "
              << input.depth << std::endl
              << "Numbler inputs: " << dimensions_.num_inputs << std::endl
              << "Number outputs: " << dimensions_.num_outputs << std::endl;
    std::exit(1);
  }
  // The input dimensions must be multiples of the output dimensions. This is
  // pooling pixels, not resizing, all pooling groups must be the same size with
  // no remainer.
  if ((input.width % output.width != 0) ||
      (input.height % output.height != 0)) {
    std::cerr << "MaxPool layer #" << layer_index
              << " specified with output height or width which is not an even "
                 "divisor of the input dimensions"
              << std::endl;
  }
}

std::tuple<size_t, size_t, size_t> MaxPoolLayer::GetOutputDimensions(
    const VolumeDimensions& dim, const AreaDimensions& output) {
  return std::make_tuple(output.width, output.height, dim.depth);
}

symbolic::Expression MaxPoolLayer::GenerateOutputCode(
    const symbolic::Expression& index) const {
  // Get 3D output dimensions. (output will be a 1D serialized form of this,
  // using mapping output_flat_index).
  std::tuple<size_t, size_t, size_t> output_dims =
      GetOutputDimensions(input_, target_);
  size_t output_width = std::get<0>(output_dims);
  size_t output_height = std::get<1>(output_dims);
  size_t output_depth = std::get<2>(output_dims);

  size_t group_width = input_.width / output_width;
  size_t group_height = input_.height / output_height;

  symbolic::Expression output_row = symbolic::Unflatten3dRow(
      output_width, output_height, output_depth, index);

  symbolic::Expression output_col = symbolic::Unflatten3dCol(
      output_width, output_height, output_depth, index);

  symbolic::Expression output_z = symbolic::Unflatten3dPlane(
      output_width, output_height, output_depth, index);

  std::vector<symbolic::Expression> group;
  symbolic::Expression group_r_start = output_row * group_height;
  symbolic::Expression group_c_start = output_col * group_width;
  for (size_t group_r = 0; group_r < group_height; ++group_r) {
    for (size_t group_c = 0; group_c < group_width; ++group_c) {
      group.push_back(
          generator_.I(group_r_start + group_r, group_c_start, output_z));
    }
  }
  symbolic::Expression group_max = symbolic::Max(group);
  return group_max;
}

Matrix<symbolic::Expression> MaxPoolLayer::InputGradientsForOutput(const symbolic::Expression& index) const {
  // Get 3D output dimensions. (output will be a 1D serialized form of this,
  // using mapping output_flat_index).
  std::tuple<size_t, size_t, size_t> output_dims =
      GetOutputDimensions(input_, target_);
  size_t output_width = std::get<0>(output_dims);
  size_t output_height = std::get<1>(output_dims);
  size_t output_depth = std::get<2>(output_dims);
  size_t group_width = input_.width / output_width;
  size_t group_height = input_.height / output_height;

  symbolic::Expression output = GenerateOutputCode(index);

  symbolic::Expression output_row = symbolic::Unflatten3dRow(
      output_width, output_height, output_depth, index);

  symbolic::Expression output_col = symbolic::Unflatten3dCol(
      output_width, output_height, output_depth, index);

  symbolic::Expression output_z = symbolic::Unflatten3dPlane(
      output_width, output_height, output_depth, index);

  Matrix<symbolic::Expression> group_gradients(group_height, group_width);
  symbolic::Expression group_r_start = output_row * group_height;
  symbolic::Expression group_c_start = output_col * group_width;
  for (size_t group_r = 0; group_r < group_height; ++group_r) {
    for (size_t group_c = 0; group_c < group_width; ++group_c) {
      group_gradients = output.Derive(
          generator_.I(group_r + group_r_start, group_c_start, output_z));
    }
  }
  return group_gradients;
}

symbolic::Expression MaxPoolLayer::InputGradientCode(
    const symbolic::Expression& input_index) const {
  // Get 3D output dimensions.
  std::tuple<size_t, size_t, size_t> output_dims =
      GetOutputDimensions(input_, target_);
  size_t output_width = std::get<0>(output_dims);
  size_t output_height = std::get<1>(output_dims);
  size_t output_depth = std::get<2>(output_dims);
  size_t group_width = input_.width / output_width;
  size_t group_height = input_.height / output_height;

  symbolic::Expression input_row = symbolic::Unflatten3dRow(
      input_.width, input_.height, input_.depth, index);

  symbolic::Expression input_col = symbolic::Unflatten3dCol(
      input_.width, input_.height, input_.depth, index);

  symbolic::Expression input_z = symbolic::Unflatten3dPlane(
      input_.width, input_.height, input_depth, index);

  symbolic::Expression output_row = input_row/group_height;
  symbolic::Expression output_col = input_col/group_width;
  symbolic::Expression output_z = input_z;
  symbolic::Expression output_index = symbolic::Flatten3d(output_width, output_height, output_depth, output_row, output_col, output_z);

  Matrix<symbolic::Expression> input_gradients = InputGradientsForOutput(output_index);

  // Conveniently, Matrix<symbolic::Expression>::to_string() gives us C-like
  // initializer list syntax.
  std::string input_gradient_array = input_gradients.to_string();

  // Ah... shoot this is a hard problem. Alright, use codegen CudaGenerator to
  // create code like...
  
  // float group[group_height][group_width] = input_gradient_array.to_string();

  // Then use CudaGenerator to 2d access from the group the input's coordinates
  // within it's group (get the origin of the group and subtract input from
  // group).
  //
  // This approach suffers from the problem of redundant calculation -- each
  // thread calculates the gradients of all the inputs in its group for each
  // input. Need to have a better approach, but since pooling is probably only
  // giving groupsizes of 16x16, it might be okay?

}

symbolic::Expression SoftmaxLayer::WeightGradientCode(
    const symbolic::Expression& weight_index) const {
  return symbolic::Expression(0.0);   
}

std::unique_ptr<LayerImpl> MaxPoolLayer::Clone() const {
  return std::make_unique<MaxPoolLayer>(input_, target_, layer_index_);
}

}  // namespace nnet
