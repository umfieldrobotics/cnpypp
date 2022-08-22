#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <tuple>

#include <tuple_util.hpp>

using tuple_t = std::pair<float, uint32_t>;

int main() {
  //~ std::cout << offsetof(tuple_t, first) << " " << offsetof(tuple_t, second)
  //<< std::endl;

  std::array<std::byte, 8 * 20> arr;

  for (int i = 0; i < 20; ++i) {
    auto* ptr = &arr[8 * i];
    float* f = reinterpret_cast<float*>(ptr + offsetof(tuple_t, first));
    uint32_t* u = reinterpret_cast<uint32_t*>(ptr + offsetof(tuple_t, second));

    *f = float(M_PI) * i;
    *u = i;
  }

  std::cout << reinterpret_cast<tuple_t*>(&arr[0])->first << "\t"
            << reinterpret_cast<tuple_t*>(&arr[0])->second << std::endl;
  std::cout << reinterpret_cast<tuple_t*>(&arr[8])->first << "\t"
            << reinterpret_cast<tuple_t*>(&arr[8])->second << std::endl;
  std::cout << reinterpret_cast<tuple_t*>(&arr[16])->first << "\t"
            << reinterpret_cast<tuple_t*>(&arr[16])->second << std::endl;
  std::cout << reinterpret_cast<tuple_t*>(&arr[24])->first << "\t"
            << reinterpret_cast<tuple_t*>(&arr[24])->second << std::endl;

  std::cout << std::endl;

  cnpypp::tuple_iterator<tuple_t> it(arr.begin());
  std::cout << (*it).first << "," << (*it).second << std::endl;

  ++it;
  std::cout << (*it).first << "," << (*it).second << std::endl;

  ++it;
  std::cout << (*it).first << "," << (*it).second << std::endl;

  cnpypp::tuple_iterator<tuple_t> const it2(arr.begin());
  auto const it3 = it2;

  std::cout << std::distance(it2, it) << std::endl;
  std::cout << std::distance(it, it2) << std::endl;
  std::cout << std::distance(it2, it3) << std::endl;
  std::cout << (*it2).first << "," << (*it2).second << std::endl;
}
