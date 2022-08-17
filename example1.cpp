#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <numeric>
#include <string>
#include <string_view>

#include <boost/iterator/zip_iterator.hpp>

#include "cnpy++.hpp"

static const int Nx = 2;
static const int Ny = 4;
static const int Nz = 8;

static const int Nelem = Nx * Ny * Nz;

static std::vector<size_t> const shape{Nz, Ny, Nx};

int main() {
  auto const data = std::invoke([]() {
    std::vector<uint32_t> data(Nx * Ny * Nz);
    std::iota(data.begin(), data.end(), 1);
    return data;
  });

  // save it to file
  cnpypp::npy_save("arr1.npy", &data[0], shape, "w");
  cnpypp::npy_save("arr1-cpy.npy", data.cbegin(), shape, "w");

  // load it into a new array
  {
    cnpypp::NpyArray const arr = cnpypp::npy_load("arr1.npy");
    auto const* const loaded_data = arr.data<uint32_t>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(data)::value_type))) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }

    if (arr.shape.size() != shape.size() ||
        !std::equal(shape.cbegin(), shape.cend(), arr.shape.cbegin())) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }

    if (!std::equal(data.cbegin(), data.cend(), loaded_data)) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }
  }

  // append the same data to file
  // npy array on file now has shape (Nz+Nz,Ny,Nx)
  cnpypp::npy_save("arr1.npy", data.cbegin(), shape, "a");

  {
    cnpypp::NpyArray const arr = cnpypp::npy_load("arr1.npy");
    auto const* const loaded_data = arr.data<uint32_t>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(data)::value_type))) {
      std::cerr << "error in line " << __LINE__ << std::endl;
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
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }

    if (!std::equal(complete_new_data.cbegin(), complete_new_data.cend(),
                    loaded_data)) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }
  }

  std::string_view const str1 = "abcdefghijklmno";
  std::string_view const str2 = "pqrstuvwxyz";

  // now write to an npz file
  {
    cnpypp::npz_save("out.npz", "str", str1.cbegin(), {str1.size()}, "w");

    // load str1 back from npz file
    cnpypp::NpyArray arr = cnpypp::npz_load("out.npz", "str");
    auto const* const loaded_data = arr.data<char>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(str1)::value_type))) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }

    if (arr.shape.size() != 1 || arr.shape.at(0) != str1.size()) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }

    if (!std::equal(str1.cbegin(), str1.cend(), loaded_data)) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }
  }

  // append to npz
  {
    cnpypp::npz_save("out.npz", "str2", str2.cbegin(), {str2.size()}, "a");

    // load str2 back from npz file
    cnpypp::NpyArray arr = cnpypp::npz_load("out.npz", "str2");
    auto const* const loaded_data = arr.data<char>();

    // make sure the loaded data matches the saved data
    if (!(arr.word_size == sizeof(decltype(str2)::value_type))) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }

    if (arr.shape.size() != 1 || arr.shape.at(0) != str2.size()) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      std::cerr << arr.shape.size() << '\n';
      return EXIT_FAILURE;
    }

    if (!std::equal(str2.cbegin(), str2.cend(), loaded_data)) {
      std::cerr << "error in line " << __LINE__ << std::endl;
      return EXIT_FAILURE;
    }
  }

  std::list<uint32_t> const list_u{data.cbegin(), data.cend()}; // copy to list
  std::list<float_t> const list_f{data.cbegin(), data.cend()};

  // append list to npz
  {
    cnpypp::npz_save("out.npz", "arr1", list_u.cbegin(), shape, "a");
    cnpypp::npz_save("out.npz", "arr2", list_f.cbegin(), shape, "a");
  }

  // load the entire npz file
  {
    cnpypp::npz_t my_npz = cnpypp::npz_load("out.npz");

    {
      cnpypp::NpyArray const& arr = my_npz.find("str")->second;
      char const* const loaded_str = arr.data<char>();

      // make sure the loaded data matches the saved data
      if (!(arr.word_size == sizeof(decltype(str1)::value_type))) {
        std::cerr << "error in line " << __LINE__ << std::endl;
        return EXIT_FAILURE;
      }

      if (arr.shape.size() != 1 || arr.shape.at(0) != str1.size()) {
        std::cerr << "error in line " << __LINE__ << std::endl;
        return EXIT_FAILURE;
      }

      if (!std::equal(str1.cbegin(), str1.cend(), loaded_str)) {
        std::cerr << "error in line " << __LINE__ << std::endl;
        return EXIT_FAILURE;
      }
    }
    {
      cnpypp::NpyArray const& arr = my_npz.find("arr1")->second;
      cnpypp::NpyArray const& arr2 = my_npz.find("arr2")->second;
      uint32_t const* const loaded_arr_u = arr.data<uint32_t>();
      float const* const loaded_arr_f = arr2.data<float>();

      // make sure the loaded data matches the saved data
      if (arr.word_size != sizeof(uint32_t) ||
          arr2.word_size != sizeof(float)) {
        std::cerr << "error in line " << __LINE__ << std::endl;
        return EXIT_FAILURE;
      }

      if (!std::equal(arr.shape.cbegin(), arr.shape.cend(), shape.cbegin()) ||
          !std::equal(arr2.shape.cbegin(), arr2.shape.cend(), shape.cbegin())) {
        std::cerr << "error in line " << __LINE__ << std::endl;
        return EXIT_FAILURE;
      }

      if (!std::equal(data.cbegin(), data.cend(), loaded_arr_u)) {
        std::cerr << "error in line " << __LINE__ << std::endl;
        return EXIT_FAILURE;
      }

      if (!std::equal(data.cbegin(), data.cend(), loaded_arr_f)) {
        std::cerr << "error in line " << __LINE__ << std::endl;
        return EXIT_FAILURE;
      }
    }
  }

  // tuples written to NPY with structured data type
  {
    std::vector<std::tuple<int32_t, int8_t, int16_t>> const tupleVec{
        {0xaaaaaaaa, 0xbb, 0xcccc},
        {0xdddddddd, 0xee, 0xffff},
        {0x99999999, 0x88, 0x7777}};

    cnpypp::npy_save("structured.npy", {"a", "b", "c"}, tupleVec.begin(),
                     {tupleVec.size()});
  }

  // std::array written as structured type
  {

    std::vector<std::array<int8_t, 2>> const arrVec{
        {0x11, 0x22}, {0x33, 0x44}, {0x55, 0x66}};

    cnpypp::npy_save("structured2.npy", {"a", "b"}, arrVec.begin(),
                     {arrVec.size()});
  }

  return EXIT_SUCCESS;
}
