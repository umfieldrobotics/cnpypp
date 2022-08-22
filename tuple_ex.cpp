#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <tuple>

#include <iomanip>
#include <iostream>

#include <cnpy++.hpp>
#include <tuple_util.hpp>

#include <boost/type_index.hpp>

using tuple_t = std::tuple<uint16_t, float>;

int main() {
  std::array<std::byte, cnpypp::tuple_info<tuple_t>::sum_sizes * 6> arr{};

  for (int i = 0; i < 6; ++i) {
    auto* ptr = &arr[cnpypp::tuple_info<tuple_t>::sum_sizes * i];
    auto* u = reinterpret_cast<uint16_t*>(
        ptr + cnpypp::tuple_info<tuple_t>::offsets[0]);
    auto* f =
        reinterpret_cast<float*>(ptr + cnpypp::tuple_info<tuple_t>::offsets[1]);

    *f = float(M_PI) * i;
    *u = i;
  }

  tuple_t tup;

  for (auto&& p : cnpypp::tuple_info<tuple_t>::offsets) {
    std::cout << p << ',' << ' ';
  }

  std::cout << &tup << " " << &(std::get<0>(tup)) << " " << &(std::get<1>(tup))
            << std::endl;

  std::cout << &arr[0] << std::endl;

  cnpypp::tuple_iterator<tuple_t> it(arr.begin());
  std::tuple<uint16_t&, float&> refs = *it;
  std::tuple<uint16_t const&, float const&> refs2 = *it;

  for (cnpypp::tuple_iterator<tuple_t> it{arr.begin()};
       it != cnpypp::tuple_iterator<tuple_t>{arr.end()}; ++it) {
    std::cout << std::get<0>(*it) << "\t" << std::get<1>(*it) << std::endl;
    std::cout << &(std::get<0>(*it)) << "\t" << &(std::get<1>(*it))
              << std::endl;
  }

  cnpypp::NpyArray npyarr{{6},
                          {sizeof(uint16_t), sizeof(float)},
                          {"uint16", "float"},
                          cnpypp::MemoryOrder::Fortran};
  std::copy(arr.cbegin(), arr.cend(), npyarr.begin<std::byte>());

  std::cout << npyarr.cbegin<std::byte>() << " " << npyarr.cend<std::byte>()
            << std::endl;

  auto r = npyarr.make_tuple_range<uint16_t, float>();
  std::cout << r.begin().ptr_ << " " << r.end().ptr_ << std::endl;
  std::cout << &(std::get<0>(*(r.begin()))) << " " << &(std::get<0>(*(r.end())))
            << std::endl;
  std::cout << "=== range-based for ===" << std::endl;
  for (auto&& t : r) {
    std::cout << &(std::get<0>(t)) << ":\t" << std::get<0>(t) << '\t'
              << std::get<1>(t) << '\n';
  }
}
