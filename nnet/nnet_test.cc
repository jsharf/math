#include "math/nnet/nnet.h"
#include "math/symbolic/expression.h"
#include "math/geometry/matrix.h"

#include <iostream>
#include <cassert>
#include <fstream>
#include <string>

std::string trim(std::string& str)
{
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last-first+1));
}

std::string GenerateRandomFlw() {
  std::string word = "";
  word += 'a' + std::rand()%26;
  word += 'a' + std::rand()%26;
  word += 'a' + std::rand()%26;
  word += 'a' + std::rand()%26;
  word += 'a' + std::rand()%26;
  return word;
}

constexpr size_t kSampleSize = 5;
using Sample = Matrix<kSampleSize, 1, Number>;

Sample ConvertToSample(std::string word) {
  Sample result;
  for (size_t i = 0; i < kSampleSize; ++i) {
    result.at(i, 0) = std::tolower(word[i]) - 'a' + 1;
  }
  return result;
}

// Trains a neural network to detect if a given 5-character string is an english
// word.
int main() {
  constexpr int kNumHiddenLayers = 1;
  constexpr int kLayerSize = 3;
  constexpr int kOutputSize = 1;
  constexpr int kInputSize = kSampleSize;

  std::cout << "Starting... " << std::endl;

  using Nnet = Nnet<kNumHiddenLayers, kLayerSize, kOutputSize, kInputSize>;
  Nnet test_net;
  std::cout << "Expr: " << std::endl << test_net.to_string() << std::endl;
 
  constexpr const char* dictionary_path = "/usr/share/dict/words";
  std::ifstream dictionary_obj(dictionary_path);

  std::unordered_map<std::string, Number> flw;

  // Add valid words to flw.
  std::string word;
  while (std::getline(dictionary_obj, word)) {
    word = trim(word);
    if (word.size() == 5) {
      flw[word] = 1;
    }
  }

  std::cout << "Valid words: " << flw.size() << std::endl;

  for (size_t i = 0; i < 5000; ++i) {
    std::string word = GenerateRandomFlw();
    if (flw.count(word) == 0) {
      flw[word] = 0;
    }
  }

  std::cout << "Samples generated: " << flw.size() << std::endl;

  Nnet::LearningParameters params {
    .learning_rate = 0.1,
  };

  std::cout << "Training" << std::endl;
  for (const auto& example : flw) {
    std::cout << "." << std::flush;
    test_net.Train(ConvertToSample(example.first), Nnet::OutputVector({{example.second}}), params);
  }
  std::cout << "Weights: " << std::endl;
  std::cout << test_net.WeightsToString();
  std::cout << std::endl;
}