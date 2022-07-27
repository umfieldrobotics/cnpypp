#include <cnpy++.hpp>

#include <iostream>
#include <tuple>

int main() {
  std::tuple<int, double> t1;
  std::array<float, 5> t2;
  
  if constexpr (cnpypp::detail::tuple_like<decltype(t1)>) {
    std::cout << "t1 is tuple-like\n";
  }
  if constexpr (cnpypp::detail::tuple_like<decltype(t2)>) {
    std::cout << "t2 is tuple-like\n";
  }
}
