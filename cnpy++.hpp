// Copyright (C) 2011  Carl Rogers, 2020-2022 Maximilian Reininghaus
// Released under MIT License
// license available in LICENSE file, or at
// http://www.opensource.org/licenses/mit-license.php

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <complex>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/endian/buffers.hpp>
#include <gsl/span>
#include <zlib.h>

#include <cnpy++.h>

namespace cnpypp {
namespace detail {
#if __cpp_lib_concepts >= 202002L
template <class T, std::size_t N>
concept has_tuple_element = requires(T t) {
  typename std::tuple_element_t<N, std::remove_const_t<T>>;
  { get<N>(t) } -> std::convertible_to<const std::tuple_element_t<N, T>&>;
};

template <class T>
concept tuple_like = !std::is_reference_v<T> && requires(T t) {
  typename std::tuple_size<T>::type;
  requires std::derived_from<
      std::tuple_size<T>,
      std::integral_constant<std::size_t, std::tuple_size_v<T>>>;
}
&&[]<std::size_t... N>(std::index_sequence<N...>) {
  return (has_tuple_element<T, N> && ...);
}
(std::make_index_sequence<std::tuple_size_v<T>>());
#endif

} // namespace detail

enum class MemoryOrder {
  Fortran = cnpypp_memory_order_fortran,
  C = cnpypp_memory_order_c,
  ColumnMajor = Fortran,
  RowMajor = C
};

struct NpyArray {
  NpyArray(std::vector<size_t> _shape, size_t _word_size,
           MemoryOrder _memory_order, std::unique_ptr<std::byte[]>&& _buffer,
           size_t _buffer_length)
      : shape{std::move(_shape)}, word_size{_word_size},
        memory_order{_memory_order}, num_vals{std::accumulate(
                                         shape.begin(), shape.end(), size_t{1},
                                         std::multiplies<size_t>())},
        offset{_buffer_length - num_vals * _word_size}, buffer{std::move(
                                                            _buffer)} {}

  NpyArray(NpyArray&& other)
      : shape{std::move(other.shape)}, word_size{other.word_size},
        memory_order{other.memory_order}, num_vals{other.num_vals},
        offset{other.offset}, buffer{std::move(other.buffer)} {}

  NpyArray(std::vector<size_t> const& _shape, size_t _word_size,
           MemoryOrder _memory_order)
      : NpyArray{_shape, _word_size, _memory_order,
                 std::make_unique<std::byte[]>(
                     std::accumulate(_shape.begin(), _shape.end(), _word_size,
                                     std::multiplies<size_t>())),
                 std::accumulate(_shape.begin(), _shape.end(), _word_size,
                                 std::multiplies<size_t>())} {}

  NpyArray(NpyArray const&) = delete;

  template <typename T> T* data() {
    return reinterpret_cast<T*>(&(buffer.get() + offset)[0]);
  }

  template <typename T> const T* data() const {
    return reinterpret_cast<T const*>(&(buffer.get() + offset)[0]);
  }

  size_t num_bytes() const { return num_vals * word_size; }

  bool compare_metadata(NpyArray const& other) const {
    return shape == other.shape && word_size == other.word_size &&
           memory_order == other.memory_order;
  }

  bool operator==(NpyArray const& other) const {
    return compare_metadata(other) &&
           !std::memcmp(this->data<void>(), other.data<void>(), num_bytes());
  }

  template <typename T> T* begin() { return data<T>(); }

  template <typename T> T const* cbegin() const { return data<T>(); }

  template <typename T> T* end() { return data<T>() + num_vals; }

  template <typename T> T const* cend() const { return data<T>() + num_vals; }

  std::vector<size_t> const shape;
  size_t const word_size;
  MemoryOrder const memory_order;
  size_t const num_vals;
  size_t const offset;

private:
  std::unique_ptr<std::byte[]> buffer;
};

using npz_t = std::map<std::string, NpyArray>;

template <typename F> char constexpr map_type(std::complex<F>) { return 'c'; }

template <typename T> char constexpr map_type(T) {
  static_assert(std::is_arithmetic_v<T>, "only arithmetic types supported");

  // bool not supported at the moment (-> std::vector<bool> issues etc.)
  /*if constexpr (std::is_same_v<T, bool>) {
    return 'b';
  }*/

  if constexpr (std::is_integral_v<T>) {
    return std::is_signed_v<T> ? 'i' : 'u';
  }

  if constexpr (std::is_floating_point_v<T>) {
    return 'f';
  }
}

char BigEndianTest();
bool _exists(std::string const&); // calls boost::filesystem::exists()

std::vector<char> create_npy_header(gsl::span<size_t const> shape, char dtype,
                                    int size, MemoryOrder = MemoryOrder::C);

std::vector<char> create_npy_header(gsl::span<size_t const> shape,
                                    gsl::span<std::string_view const> labels,
                                    gsl::span<char const> dtypes,
                                    gsl::span<size_t const> sizes,
                                    MemoryOrder memory_order);

void parse_npy_header(std::istream&, size_t& word_size,
                      std::vector<size_t>& shape, MemoryOrder& memory_order);

void parse_npy_header(std::istream::char_type* buffer, size_t& word_size,
                      std::vector<size_t>& shape, MemoryOrder& memory_order);

void parse_zip_footer(std::istream& fp, uint16_t& nrecs,
                      uint32_t& global_header_size,
                      uint32_t& global_header_offset);

npz_t npz_load(std::string const& fname);

NpyArray npz_load(std::string const& fname, std::string const& varname);

NpyArray npy_load(std::string const& fname);

namespace detail {
template <template <typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

template <template <typename...> class Template, typename... Args>
bool constexpr is_specialization_of_v =
    is_specialization_of<Template, Args...>::value;

template<class T>
struct is_std_array : std::false_type {};

template<class T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
bool constexpr is_std_array_v = is_std_array<T>::value;
} // namespace detail

template <typename Tup> struct tuple_info {
  static_assert(
      detail::is_specialization_of_v<std::tuple, Tup> ||
          detail::is_specialization_of_v<std::pair, Tup> || detail::is_std_array_v<Tup>,
      "must provide std::tuple-like type"); // TODO: check/allow std::array<>

  static auto constexpr size = std::tuple_size_v<Tup>;

private:
  static std::array<char, size> constexpr getDataTypes() {
    std::array<char, size> types{};
    getDataTypes_impl<0>(types);
    return types;
  }

  static std::array<size_t, size> constexpr getElementSizes() {
    std::array<size_t, size> sizes{};
    getSizes_impl<0>(sizes);
    return sizes;
  }

public:
  static std::array<char, size> constexpr data_types = getDataTypes();
  static std::array<size_t, size> constexpr element_sizes = getElementSizes();

private:
  static size_t constexpr sum_size_impl() {
    size_t sum{};

    for (auto const& v : element_sizes) {
      sum += v;
    }

    return sum;
  }

public:
  static size_t constexpr sum_sizes = sum_size_impl();

private:
  static std::array<size_t, size> constexpr calc_offsets() {
    std::array<size_t, size> offsets{};
    offsets[0] = 0;
    calc_offsets_impl<1>(offsets);
    return offsets;
  }

public:
  static std::array<size_t, size> constexpr offsets = calc_offsets();

private:
  template <int k>
  static void constexpr getDataTypes_impl(std::array<char, size>& sizes) {
    if constexpr (k < size) {
      sizes[k] = map_type(std::tuple_element_t<k, Tup>{});
      getDataTypes_impl<k + 1>(sizes);
    }
  }

  template <int k>
  static void constexpr getSizes_impl(std::array<size_t, size>& sizes) {
    if constexpr (k < size) {
      sizes[k] = sizeof(std::tuple_element_t<k, Tup>);
      getSizes_impl<k + 1>(sizes);
    }
  }

  template <int k>
  static void constexpr calc_offsets_impl(std::array<size_t, size>& offsets) {
    if constexpr (k < size) {
      offsets[k] = offsets[k - 1] + element_sizes[k - 1];
      calc_offsets_impl<k + 1>(offsets);
    }
  }
};

template <typename TConstInputIterator>
bool constexpr is_contiguous_v =
#if __cpp_lib_concepts >= 202002L
    std::contiguous_iterator<TConstInputIterator>;
#else
#warning "no concept support available - fallback to std::is_pointer_v"
    std::is_pointer_v<TConstInputIterator>; // unfortunately, is_pointer is less
                                            // sharp (e.g., doesn't bite on
                                            // std::vector<>::iterator)
#endif

// if it comes from contiguous memory, dump directly in file
template <typename TConstInputIterator,
          std::enable_if_t<is_contiguous_v<TConstInputIterator>, int> = 0>
void write_data(TConstInputIterator start, size_t nels, std::ostream& fs) {
  using value_type =
      typename std::iterator_traits<TConstInputIterator>::value_type;

  auto constexpr size_elem = sizeof(value_type);

  fs.write(reinterpret_cast<std::ostream::char_type const*>(&*start),
           nels * size_elem / sizeof(std::ostream::char_type));
}

// otherwise do it in chunks with a buffer
template <typename TConstInputIterator,
          std::enable_if_t<!is_contiguous_v<TConstInputIterator>, int> = 0>
void write_data(TConstInputIterator start, size_t nels, std::ostream& fs) {
  using value_type =
      typename std::iterator_traits<TConstInputIterator>::value_type;

  size_t constexpr buffer_size = 0x10000;

  auto buffer = std::make_unique<value_type[]>(buffer_size);

  size_t elements_written = 0;
  auto it = start;

  while (elements_written < nels) {
    size_t count = 0;
    while (count < buffer_size && elements_written < nels) {
      buffer[count] = *it;
      ++it;
      ++count;
      ++elements_written;
    }
    write_data(buffer.get(), count, fs);
  }
}

template <typename T, int k=0>
void fill(T const& tup, char* buffer) {
  auto constexpr offsets = tuple_info<T>::offsets;

  if constexpr (k < tuple_info<T>::size) {
    auto const& elem = std::get<k>(tup);
    auto constexpr elem_size = sizeof(elem);
    static_assert(tuple_info<T>::element_sizes[k] == elem_size); // sanity check

    char const* const beg = reinterpret_cast<char const*>(&elem);

    std::copy(beg, beg + elem_size, buffer + offsets[k]);
    fill<T, k+1>(tup, buffer);
  }
}

template <typename TTupleIterator>
void write_data_tuple(TTupleIterator start, size_t nels, std::ostream& fs) {
  using value_type = typename std::iterator_traits<TTupleIterator>::value_type;
  static auto constexpr sizes = tuple_info<value_type>::element_sizes;
  static auto constexpr sum = tuple_info<value_type>::sum_sizes;
  static auto constexpr offsets = tuple_info<value_type>::offsets;

  size_t constexpr buffer_size = 0x10000; // number of tuples

  auto buffer = std::make_unique<char[]>(buffer_size * sum);

  size_t elements_written = 0;
  auto it = start;

  while (elements_written < nels) {
    size_t count = 0;
    while (count < buffer_size && elements_written < nels) {
      auto const& tup = *it;

      fill<value_type>(tup, buffer.get() + count * sum);

      ++it;
      ++count;
      ++elements_written;
    }
    write_data(buffer.get(), count * sum, fs);
  }
}

template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
std::vector<char>& operator+=(std::vector<char>& lhs, const T rhs) {
  // write in little endian
  boost::endian::endian_buffer<boost::endian::order::little, T,
                               sizeof(T) * CHAR_BIT> const buffer{rhs};

  for (auto const* ptr = buffer.data(); ptr < buffer.data() + sizeof(T);
       ++ptr) {
    lhs.push_back(*ptr);
  }

  return lhs;
}

std::vector<char>& append(std::vector<char>&, std::string_view);

template <typename TConstInputIterator>
void npy_save(std::string const& fname, TConstInputIterator start,
              gsl::span<size_t const> const shape, std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  std::fstream fs;
  std::vector<size_t>
      true_data_shape; // if appending, the shape of existing + new data

  using value_type =
      typename std::iterator_traits<TConstInputIterator>::value_type;

  if (mode == "a" && _exists(fname)) {
    // file exists. we need to append to it. read the header, modify the array
    // size
    fs.open(fname,
            std::ios_base::binary | std::ios_base::in | std::ios_base::out);

    size_t word_size;
    MemoryOrder memory_order_exist;
    parse_npy_header(fs, word_size, true_data_shape, memory_order_exist);
    if (memory_order != memory_order_exist) {
      throw std::runtime_error{
          "libcnpy++ error in npy_save(): memory order does not match"};
    }

    if (word_size != sizeof(value_type)) {
      std::stringstream ss;
      ss << "libnpy error: " << fname << " has word size " << word_size
         << " but npy_save appending data sized " << sizeof(value_type);
      throw std::runtime_error{ss.str().c_str()};
    }

    if (true_data_shape.size() != shape.size()) {
      std::stringstream ss;
      std::cerr << "libnpy error: npy_save attempting to append misdimensioned "
                   "data to "
                << fname;
      throw std::runtime_error{ss.str().c_str()};
    }

    if (!std::equal(std::next(shape.begin()), shape.end(),
                    std::next(true_data_shape.begin()))) {
      std::stringstream ss;
      ss << "libnpy error: npy_save attempting to append misshaped data to "
         << fname;
      throw std::runtime_error{ss.str().c_str()};
    }

    true_data_shape[0] += shape[0];

  } else { // write mode
    fs.open(fname, std::ios_base::binary | std::ios_base::out);
    true_data_shape = std::vector<size_t>{shape.begin(), shape.end()};
  }

  std::vector<char> const header =
      create_npy_header(true_data_shape, map_type(value_type{}),
                        sizeof(value_type), memory_order);
  size_t const nels =
      std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());

  fs.seekp(0, std::ios_base::beg);
  fs.write(&header[0], sizeof(char) * header.size());
  fs.seekp(0, std::ios_base::end);

  // now write actual data
  write_data(start, nels, fs);
}

template <typename TConstInputIterator>
void npy_save(std::string const& fname, TConstInputIterator start,
              std::initializer_list<size_t> const shape,
              std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  npy_save<TConstInputIterator>(fname, start,
                                gsl::span{std::data(shape), shape.size()}, mode,
                                memory_order);
}

template <typename TConstInputIterator>
void npz_save(std::string const& zipname, std::string fname,
              TConstInputIterator start, gsl::span<size_t const> const shape,
              std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  using value_type =
      typename std::iterator_traits<TConstInputIterator>::value_type;

  // first, append a .npy to the fname
  fname += ".npy";

  // now, on with the show
  std::fstream fs;
  uint16_t nrecs = 0;
  uint32_t global_header_offset = 0;
  std::vector<char> global_header;

  if (mode == "a" && _exists(zipname)) {
    fs.open(zipname,
            std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    // zip file exists. we need to add a new npy file to it.
    // first read the footer. this gives us the offset and size of the global
    // header then read and store the global header. below, we will write the
    // the new data at the start of the global header then append the global
    // header and footer below it
    uint32_t global_header_size;
    parse_zip_footer(fs, nrecs, global_header_size, global_header_offset);
    fs.seekp(global_header_offset, std::ios_base::beg);
    global_header.resize(global_header_size);
    fs.read(&global_header[0], sizeof(char) * global_header_size);
    if (fs.gcount() != global_header_size) {
      throw std::runtime_error(
          "npz_save: header read error while adding to existing zip");
    }
    fs.seekp(global_header_offset, std::ios_base::beg);
  } else {
    fs.open(zipname, std::ios_base::out | std::ios_base::binary);
  }

  std::vector<char> npy_header = create_npy_header(
      shape, map_type(value_type{}), sizeof(value_type), memory_order);

  size_t nels =
      std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());
  size_t nbytes = nels * sizeof(value_type) + npy_header.size();

  // get the CRC of the data to be added
  uint32_t crc = _crc32(0L, (uint8_t*)&npy_header[0], npy_header.size());

  auto it = start;
  for (size_t i = 0; i < nels; ++i) {
    auto data = *it;
    ++it;
    static_assert(std::is_same_v<decltype(data), value_type>);
    crc = _crc32(crc, (uint8_t*)(&data), sizeof(value_type));
  }

  // build the local header
  std::vector<char> local_header;
  append(local_header, "PK");             // first part of sig
  local_header += (uint16_t)0x0403;       // second part of sig
  local_header += (uint16_t)20;           // min version to extract
  local_header += (uint16_t)0;            // general purpose bit flag
  local_header += (uint16_t)0;            // compression method
  local_header += (uint16_t)0;            // file last mod time
  local_header += (uint16_t)0;            // file last mod date
  local_header += (uint32_t)crc;          // crc
  local_header += (uint32_t)nbytes;       // compressed size
  local_header += (uint32_t)nbytes;       // uncompressed size
  local_header += (uint16_t)fname.size(); // fname length
  local_header += (uint16_t)0;            // extra field length
  append(local_header, fname);

  // build global header
  append(global_header, "PK");       // first part of sig
  global_header += (uint16_t)0x0201; // second part of sig
  global_header += (uint16_t)20;     // version made by
  global_header.insert(global_header.end(), local_header.begin() + 4,
                       local_header.begin() + 30);
  global_header += (uint16_t)0; // file comment length
  global_header += (uint16_t)0; // disk number where file starts
  global_header += (uint16_t)0; // internal file attributes
  global_header += (uint32_t)0; // external file attributes
  global_header += (uint32_t)
      global_header_offset; // relative offset of local file header, since it
                            // begins where the global header used to begin
  append(global_header, fname);

  // build footer
  std::vector<char> footer;
  append(footer, "PK");                     // first part of sig
  footer += (uint16_t)0x0605;               // second part of sig
  footer += (uint16_t)0;                    // number of this disk
  footer += (uint16_t)0;                    // disk where footer starts
  footer += (uint16_t)(nrecs + 1);          // number of records on this disk
  footer += (uint16_t)(nrecs + 1);          // total number of records
  footer += (uint32_t)global_header.size(); // nbytes of global headers
  footer += (uint32_t)(global_header_offset + nbytes +
                       local_header.size()); // offset of start of global
                                             // headers, since global header now
                                             // starts after newly written array
  footer += (uint16_t)0;                     // zip file comment length

  // write everything
  fs.write(&local_header[0], sizeof(char) * local_header.size());
  fs.write(&npy_header[0], sizeof(char) * npy_header.size());

  // write actual data
  write_data(start, nels, fs);

  fs.write(&global_header[0], sizeof(char) * global_header.size());
  fs.write(&footer[0], sizeof(char) * footer.size());
}

template <typename TConstInputIterator>
void npz_save(std::string const& zipname, std::string fname,
              TConstInputIterator start,
              std::initializer_list<size_t const> shape,
              std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  npz_save(zipname, std::move(fname), start,
           gsl::span{std::data(shape), shape.size()}, mode, memory_order);
}

template <typename TForwardIterator>
void npy_save(std::string const& fname, TForwardIterator first,
              TForwardIterator last, std::string_view mode = "w") {
  static_assert(
      std::is_base_of_v<
          std::forward_iterator_tag,
          typename std::iterator_traits<TForwardIterator>::iterator_category>,
      "forward iterator necessary");

  auto const dist = std::distance(first, last);
  if (dist < 0) {
    throw std::runtime_error(
        "npy_save() called with negative-distance iterators");
  }

  npy_save(fname, first, {static_cast<size_t>(dist)}, mode);
}

template <typename T>
void npy_save(std::string const& fname, gsl::span<T const> data,
              std::string_view mode = "w") {
  npy_save<T>(fname, data.cbegin(), data.cend(), mode);
}

template <typename TTupleIterator>
void npy_save(std::string const& fname,
              std::vector<std::string_view> const& labels, TTupleIterator first,
              gsl::span<size_t const> const shape, std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  using value_type = typename std::iterator_traits<TTupleIterator>::value_type;

  if (labels.size() != std::tuple_size_v<value_type>) {
    throw std::runtime_error("number of labels does not match tuple size");
  }

  auto constexpr dtypes = tuple_info<value_type>::data_types;
  auto constexpr sizes = tuple_info<value_type>::element_sizes;

  std::fstream fs;
  std::vector<size_t>
      true_data_shape; // if appending, the shape of existing + new data

  if (mode == "a" && _exists(fname)) {
    // file exists. we need to append to it. read the header, modify the array
    // size
    fs.open(fname,
            std::ios_base::binary | std::ios_base::in | std::ios_base::out);

    size_t word_size;
    MemoryOrder memory_order_exist;
    parse_npy_header(fs, word_size, true_data_shape, memory_order_exist);
    if (memory_order != memory_order_exist) {
      throw std::runtime_error{
          "libcnpy++ error in npy_save(): memory order does not match"};
    }

    if (word_size != sizeof(value_type)) {
      std::stringstream ss;
      ss << "libnpy error: " << fname << " has word size " << word_size
         << " but npy_save appending data sized " << sizeof(value_type);
      throw std::runtime_error{ss.str().c_str()};
    }

    if (true_data_shape.size() != shape.size()) {
      std::stringstream ss;
      std::cerr << "libnpy error: npy_save attempting to append misdimensioned "
                   "data to "
                << fname;
      throw std::runtime_error{ss.str().c_str()};
    }

    if (!std::equal(std::next(shape.begin()), shape.end(),
                    std::next(true_data_shape.begin()))) {
      std::stringstream ss;
      ss << "libnpy error: npy_save attempting to append misshaped data to "
         << fname;
      throw std::runtime_error{ss.str().c_str()};
    }

    true_data_shape[0] += shape[0];

  } else { // write mode
    fs.open(fname, std::ios_base::binary | std::ios_base::out);
    true_data_shape = std::vector<size_t>{shape.begin(), shape.end()};
  }

  auto const header =
      create_npy_header(shape, labels, dtypes, sizes, memory_order);

  size_t const nels =
      std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());

  fs.seekp(0, std::ios_base::beg);
  fs.write(&header[0], sizeof(char) * header.size());
  fs.seekp(0, std::ios_base::end);

  // now write actual data
  write_data_tuple(first, nels, fs);
}

template <typename TTupleIterator>
void npy_save(std::string const& fname,
              std::vector<std::string_view> const& labels, TTupleIterator first,
              std::initializer_list<size_t const> shape,
              std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  npy_save<TTupleIterator>(fname, labels, first,
                           gsl::span{std::data(shape), shape.size()}, mode,
                           memory_order);
}

} // namespace cnpypp
