#include <cnpy++.hpp>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  std::ifstream fs;
  fs.open(argv[1], std::ios_base::binary | std::ios_base::in);

  std::vector<size_t> word_sizes, shape;
  std::vector<char> data_types;
  std::vector<std::string> labels;
  cnpypp::MemoryOrder memory_order;

  parse_npy_header(fs, word_sizes, data_types, labels, shape, memory_order);

  std::cout << std::boolalpha << "memory order Fortran?: "
            << (memory_order == cnpypp::MemoryOrder::Fortran) << std::endl;

  std::cout << "word sizes:" << std::endl;
  for (auto&& v : word_sizes) {
    std::cout << "  " << v << std::endl;
  }

  std::cout << "data type descriptors:" << std::endl;
  for (auto&& v : data_types) {
    std::cout << "  " << v << std::endl;
  }

  std::cout << "shape:" << std::endl;
  for (auto&& v : shape) {
    std::cout << "  " << v << std::endl;
  }

  std::cout << "labels:" << std::endl;
  for (auto&& v : labels) {
    std::cout << "  " << quoted(v) << std::endl;
  }

  return 0;
}
