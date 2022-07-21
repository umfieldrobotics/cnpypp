// Copyright (C) 2011  Carl Rogers, 2020-2022 Maximilian Reininghaus
// Released under MIT License
// license available in LICENSE file, or at
// http://www.opensource.org/licenses/mit-license.php

#pragma once

#include <algorithm>
#include <cassert>
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

#include <boost/filesystem.hpp>
#include <zlib.h>

#include <cnpy++.h>

namespace cnpypp {
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

  if constexpr (std::is_same_v<T, bool>) {
    return 'b';
  }

  if constexpr (std::is_integral_v<T>) {
    return std::is_signed_v<T> ? 'i' : 'u';
  }

  if constexpr (std::is_floating_point_v<T>) {
    return 'f';
  }
}

char BigEndianTest();

template <typename T>
std::vector<char> create_npy_header(const std::vector<size_t>& shape,
                                    MemoryOrder = MemoryOrder::C);

void parse_npy_header(std::istream&, size_t& word_size,
                      std::vector<size_t>& shape, MemoryOrder& memory_order);

void parse_npy_header(std::istream::char_type* buffer, size_t& word_size,
                      std::vector<size_t>& shape, MemoryOrder& memory_order);

void parse_zip_footer(std::istream& fp, uint16_t& nrecs,
                      size_t& global_header_size, size_t& global_header_offset);

npz_t npz_load(std::string const& fname);

NpyArray npz_load(std::string const& fname, std::string const& varname);

NpyArray npy_load(std::string const& fname);

template <typename TConstInputIterator>
void write_data(TConstInputIterator, size_t, std::ostream&);

template <typename T>
std::vector<char>& operator+=(std::vector<char>& lhs, const T rhs) {
  // write in little endian
  for (size_t byte = 0; byte < sizeof(T); byte++) {
    char val = *((char*)&rhs + byte);
    lhs.push_back(val);
  }
  return lhs;
}

std::vector<char>& append(std::vector<char>&, std::string_view);

template <typename TConstInputIterator>
void npy_save(std::string const& fname, TConstInputIterator start,
              std::vector<size_t> const& shape, std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  std::fstream fs;
  std::vector<size_t>
      true_data_shape; // if appending, the shape of existing + new data

  using value_type =
      typename std::iterator_traits<TConstInputIterator>::value_type;

  if (mode == "a" && boost::filesystem::exists(fname)) {
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

    if (!std::equal(std::next(shape.cbegin()), shape.cend(),
                    std::next(true_data_shape.begin()))) {
      std::stringstream ss;
      ss << "libnpy error: npy_save attempting to append misshaped data to "
         << fname;
      throw std::runtime_error{ss.str().c_str()};
    }

    true_data_shape[0] += shape[0];

  } else { // write mode
    fs.open(fname, std::ios_base::binary | std::ios_base::out);
    true_data_shape = shape;
  }

  std::vector<char> const header =
      create_npy_header<value_type>(true_data_shape, memory_order);
  size_t const nels =
      std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());

  fs.seekp(0, std::ios_base::beg);
  fs.write(&header[0], sizeof(char) * header.size());
  fs.seekp(0, std::ios_base::end);

  // now write actual data
  write_data(start, nels, fs);
}

template <typename TConstInputIterator>
void write_data(TConstInputIterator start, size_t nels, std::ostream& fs) {
  using value_type =
      typename std::iterator_traits<TConstInputIterator>::value_type;

  // if it comes from contiguous memory, dump directly in file
#if __cplusplus >= 202002L
  if constexpr (std::contiguous_iterator<TConstInputIterator>) {
#else
  if constexpr (std::is_pointer_v<TConstInputIterator>) {
    // unfortunately, is_pointer is less sharp (e.g., doesn't bite on
    // std::vector<>::iterator)
#endif
    fs.write(reinterpret_cast<std::ostream::char_type const*>(&*start),
             nels * sizeof(value_type) / sizeof(std::ostream::char_type));
    return;
  }

  // otherwise do it in chunks with a buffer

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
    fs.write(reinterpret_cast<std::ostream::char_type*>(buffer.get()),
             sizeof(value_type) / sizeof(std::ostream::char_type) * count);
  }
}

template <typename TConstInputIterator>
void npz_save(std::string const& zipname, std::string fname,
              TConstInputIterator start, const std::vector<size_t>& shape,
              std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C) {
  using value_type =
      typename std::iterator_traits<TConstInputIterator>::value_type;

  // first, append a .npy to the fname
  fname += ".npy";

  // now, on with the show
  std::fstream fs;
  uint16_t nrecs = 0;
  size_t global_header_offset = 0;
  std::vector<char> global_header;

  if (mode == "a" && boost::filesystem::exists(zipname)) {
    fs.open(zipname,
            std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    // zip file exists. we need to add a new npy file to it.
    // first read the footer. this gives us the offset and size of the global
    // header then read and store the global header. below, we will write the
    // the new data at the start of the global header then append the global
    // header and footer below it
    size_t global_header_size;
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

  std::vector<char> npy_header =
      create_npy_header<value_type>(shape, memory_order);

  size_t nels =
      std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());
  size_t nbytes = nels * sizeof(value_type) + npy_header.size();

  // get the CRC of the data to be added
  uint32_t crc = crc32(0L, (uint8_t*)&npy_header[0], npy_header.size());

  auto it = start;
  for (size_t i = 0; i < nels; ++i) {
    auto data = *it;
    ++it;
    static_assert(std::is_same_v<decltype(data), value_type>);
    crc = crc32(crc, (uint8_t*)(&data), sizeof(value_type));
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
void npy_save(std::string const& fname, TConstInputIterator first,
              TConstInputIterator last, std::string_view mode = "w") {
  npy_save(fname, first, {std::distance(first, last)}, mode);
}

template <typename T>
void npz_save(std::string const& zipname, std::string_view fname,
              const std::vector<T>& data, std::string_view mode = "w") {
  npz_save(zipname, fname, &data[0], {data.size()}, mode);
}

template <typename T>
std::vector<char> create_npy_header(const std::vector<size_t>& shape,
                                    MemoryOrder memory_order) {
  std::vector<char> dict;
  append(dict, "{'descr': '");
  dict += BigEndianTest();
  dict += map_type(T{});
  append(dict, std::to_string(sizeof(T)));
  append(dict, "', 'fortran_order': ");
  append(dict, (memory_order == MemoryOrder::C) ? "False" : "True");
  append(dict, ", 'shape': (");
  append(dict, std::to_string(shape[0]));
  for (size_t i = 1; i < shape.size(); i++) {
    append(dict, ", ");
    append(dict, std::to_string(shape[i]));
  }
  if (shape.size() == 1)
    append(dict, ",");
  append(dict, "), }");
  // pad with spaces so that preamble+dict is modulo 16 bytes. preamble is 10
  // bytes. dict needs to end with \n
  int remainder = 16 - (10 + dict.size()) % 16;
  dict.insert(dict.end(), remainder, ' ');
  dict.back() = '\n';

  std::vector<char> header;
  header += (char)0x93;
  append(header, "NUMPY");
  header += (char)0x01; // major version of numpy format
  header += (char)0x00; // minor version of numpy format
  header += (uint16_t)dict.size();
  header.insert(header.end(), dict.begin(), dict.end());

  return header;
}

} // namespace cnpypp
