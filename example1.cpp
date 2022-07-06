#include "cnpy.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <string_view>

static const int Nx = 2;
static const int Ny = 4;
static const int Nz = 8;

static std::vector<size_t> const shape{Nz, Ny, Nx};

int main() {
  auto const data = std::invoke([]() {
    std::vector<uint32_t> data(Nx * Ny * Nz);
    std::iota(data.begin(), data.end(), 1);
    return data;
  });

  // save it to file
  cnpy::npy_save("arr1.npy", &data[0], shape, "w");
  cnpy::npy_save("arr1-cpy.npy", data.cbegin(), shape, "w");

  // load it into a new array
  {
    cnpy::NpyArray const arr = cnpy::npy_load("arr1.npy");
    auto const *const loaded_data = arr.data<uint32_t>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(data)::value_type))) {
      return EXIT_FAILURE;
    }

    if (arr.shape.size() != shape.size() ||
        !std::equal(shape.cbegin(), shape.cend(), arr.shape.cbegin())) {
      return EXIT_FAILURE;
    }

    if (!std::equal(data.cbegin(), data.cend(), loaded_data)) {
      return EXIT_FAILURE;
    }
  }

  // append the same data to file
  // npy array on file now has shape (Nz+Nz,Ny,Nx)
  cnpy::npy_save("arr1.npy", data.cbegin(), shape, "a");

  {
    cnpy::NpyArray const arr = cnpy::npy_load("arr1.npy");
    auto const *const loaded_data = arr.data<uint32_t>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(data)::value_type))) {
      return EXIT_FAILURE;
    }

    auto constexpr new_shape = std::array{16, 4, 2};
    auto constexpr new_size =
        new_shape[0] * new_shape[1] *
        new_shape[2]; // std::accumulate(new_shape.begin(), new_shape.end(), 1,
                      // std::multiplies<size_t>());
    auto const complete_new_data = std::invoke([new_size]() {
      std::vector<uint32_t> data(new_size);
      std::iota(data.begin(), std::next(data.begin(), new_size / 2), 1);
      std::iota(std::next(data.begin(), new_size / 2), data.end(), 1);
      return data;
    });

    if (arr.shape.size() != shape.size() ||
        !std::equal(new_shape.cbegin(), new_shape.cend(), arr.shape.cbegin())) {
      return EXIT_FAILURE;
    }

    if (!std::equal(complete_new_data.cbegin(), complete_new_data.cend(),
                    loaded_data)) {
      return EXIT_FAILURE;
    }
  }

  // now write to an npz file
  std::string_view const str1 = "abcdefghijklmno";
  {
    std::string_view const str1 = "abcdefghijklmno";

    cnpy::npz_save("out.npz", "str", str1.cbegin(), {str1.size()}, "w");

    // load str2 back from npz file
    cnpy::NpyArray arr = cnpy::npz_load("out.npz", "str");
    auto const *const loaded_data = arr.data<char>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(str1)::value_type))) {
      return EXIT_FAILURE;
    }

    if (arr.shape.size() != 1 || arr.shape.at(0) != str1.size()) {
      return EXIT_FAILURE;
    }

    if (!std::equal(str1.cbegin(), str1.cend(), loaded_data)) {
      return EXIT_FAILURE;
    }
  }

  std::string_view const str2 = "pqrstuvwxyz";
  // append to npz
  {
    // load str2 back from npz file
    cnpy::NpyArray arr = cnpy::npz_load("out.npz", "str");
    auto const *const loaded_data = arr.data<char>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(str2)::value_type))) {
      return EXIT_FAILURE;
    }

    if (arr.shape.size() != 1 || arr.shape.at(0) != str2.size()) {
      return EXIT_FAILURE;
    }

    if (!std::equal(str2.cbegin(), str2.cend(), loaded_data)) {
      return EXIT_FAILURE;
    }
  }

  // load the entire npz file
  {
    cnpy::npz_t my_npz = cnpy::npz_load("out.npz");

    cnpy::NpyArray const &arr = my_npz.find("str")->second;
    char const *const loaded_str = arr.data<char>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(str1)::value_type))) {
      return EXIT_FAILURE;
    }

    if (arr.shape.size() != 1 || arr.shape.at(0) != str1.size()) {
      return EXIT_FAILURE;
    }

    if (!std::equal(str1.cbegin(), str1.cend(), loaded_str)) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
