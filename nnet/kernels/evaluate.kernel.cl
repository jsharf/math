// THIS FILE IS A TEMPLATE. The files in //plasticity/nnet parse this to generate an OpenCl kernel.

// This function is generated automatically, do not edit.
double Calculate_LAYERID(global double* I, global double* W, int output_index) {
  EXPRESSION_HERE
}

kernel void evaluate_LAYERID(global double* inputs, global double* weights, global double* outputs) {
  size_t index = get_global_id(0);
  outputs[index] = Calculate_LAYERID(inputs, weights, index);
}
