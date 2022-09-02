// Copyright (C) 2011  Carl Rogers, 2020-2022 Maximilian Reininghaus
// Released under MIT License
// license available in LICENSE file, or at
// http://www.opensource.org/licenses/mit-license.php

#include <algorithm>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <stdint.h>

#include <boost/endian/conversion.hpp>
#include <boost/filesystem.hpp>
#include <gsl/span>

#include "cnpy++.hpp"

using namespace cnpypp;

char cnpypp::BigEndianTest() {
  int32_t const x = 1;
  static_assert(sizeof(x) > 1);

  return (((char*)&x)[0]) ? '<' : '>';
}

std::vector<char>& cnpypp::append(std::vector<char>& vec,
                                  std::string_view view) {
  vec.insert(vec.end(), view.begin(), view.end());
  return vec;
}

bool cnpypp::_exists(std::string const& fname) {
  return boost::filesystem::exists(fname);
}

static std::regex const num_regex("[0-9][0-9]*");
static std::regex const
    dtype_tuple_regex("\\('(\\w+)', '([<>|])([a-zA-z])(\\d+)'\\)");

void cnpypp::parse_npy_header(std::istream::char_type const* buffer,
                              std::vector<size_t>& word_sizes,
                              std::vector<char>& data_types,
                              std::vector<std::string>& labels,
                              std::vector<size_t>& shape,
                              cnpypp::MemoryOrder& memory_order) {
  uint8_t const major_version = *reinterpret_cast<uint8_t const*>(buffer + 6);
  uint8_t const minor_version = *reinterpret_cast<uint8_t const*>(buffer + 7);
  uint16_t const header_len =
      boost::endian::endian_load<boost::uint16_t, 2,
                                 boost::endian::order::little>(
          (unsigned char const*)buffer + 8);
  gsl::span header{reinterpret_cast<char const*>(buffer + 0x0a), header_len};

  if (!(major_version == 1 && minor_version == 0)) {
    throw std::runtime_error("parse_npy_header: version not supported");
  }

  parse_npy_dict(header, word_sizes, data_types, labels, shape, memory_order);
}

static std::string_view const npy_magic_string = "\x93NUMPY";

void cnpypp::parse_npy_header(std::istream& fs, std::vector<size_t>& word_sizes,
                              std::vector<char>& data_types,
                              std::vector<std::string>& labels,
                              std::vector<size_t>& shape,
                              cnpypp::MemoryOrder& memory_order) {
  auto const pos_start = fs.tellg();

  std::array<std::istream::char_type, 10> buffer;
  fs.read(buffer.data(), 10);

  if (!std::equal(npy_magic_string.begin(), npy_magic_string.end(),
                  buffer.cbegin())) {
    throw std::runtime_error("parse_npy_header: NPY magic string not found");
  }

  uint8_t const major_version = buffer[6];
  uint8_t const minor_version = buffer[7];

  if (major_version != 1 || minor_version != 0) {
    throw std::runtime_error(
        "parse_npy_header: NPY format version not supported");
  }

  uint16_t const header_len =
      boost::endian::endian_load<boost::uint16_t, 2,
                                 boost::endian::order::little>(
          reinterpret_cast<unsigned char*>(&buffer[8]));

  auto const header_buffer =
      std::make_unique<std::istream::char_type[]>(header_len);
  fs.read(header_buffer.get(), header_len);

  parse_npy_dict(gsl::span(header_buffer.get(), header_len), word_sizes,
                 data_types, labels, shape, memory_order);
}

void cnpypp::parse_npy_dict(gsl::span<std::istream::char_type const> buffer,
                            std::vector<size_t>& word_sizes,
                            std::vector<char>& data_types,
                            std::vector<std::string>& labels,
                            std::vector<size_t>& shape,
                            cnpypp::MemoryOrder& memory_order) {
  if (buffer.back() != '\n') {
    throw std::runtime_error("invalid header: missing terminating newline");
  } else if (buffer.front() != '{') {
    throw std::runtime_error("invalid header: malformed dictionary");
  }

  std::string_view const dict{buffer.data(), buffer.size()};

  if (std::cmatch matches;
      !std::regex_search(dict.begin(), dict.end(), matches,
                         std::regex{"'fortran_order': (True|False)"})) {
    throw std::runtime_error("invalid header: missing 'fortran_order'");
  } else {
    memory_order = (matches[1].str() == "True") ? cnpypp::MemoryOrder::Fortran
                                                : cnpypp::MemoryOrder::C;
  }

  word_sizes.clear();
  data_types.clear();
  labels.clear();
  shape.clear();

  // read & fill shape

  std::string_view const sh = "'shape': (";

  if (auto const pos_start_shape = dict.find(sh);
      pos_start_shape == std::string_view::npos) {
    throw std::runtime_error("invalid header: missing 'shape'");
  } else {
    if (auto const pos_end_shape = dict.find(')', pos_start_shape);
        pos_end_shape == std::string_view::npos) {
      throw std::runtime_error("invalid header: malformed dictionary");
    } else {
      std::regex digit_re{"\\d+"};
      auto dims_begin =
          std::cregex_iterator(dict.begin() + pos_start_shape,
                               dict.begin() + pos_end_shape, digit_re);
      auto dims_end = std::cregex_iterator();

      for (std::cregex_iterator it = dims_begin; it != dims_end; ++it) {
        shape.push_back(std::stoi(it->str()));
      }
    }
  }

  std::string_view const desc = "'descr': ";
  if (auto const pos_start_desc = dict.find(desc);
      pos_start_desc == std::string_view::npos) {
    throw std::runtime_error("invalid header: missing 'descr'");
  } else {
    if (auto const c = dict[pos_start_desc + desc.size()]; c == '\'') {
      // simple type

      if (std::cmatch matches;
          !std::regex_search(dict.begin() + pos_start_desc, dict.end(), matches,
                             std::regex{"'([<>\\|])([a-zA-z])(\\d+)'"})) {
        throw std::runtime_error(
            "parse_npy_header: could not parse data type descriptor");
      } else if (matches[1].str() == ">") {
        throw std::runtime_error("parse_npy_header: data stored in big-endian "
                                 "format (not supported)");
      } else {
        data_types.push_back(*(matches[2].first));
        word_sizes.push_back(std::stoi(matches[3].str()));
      }
    } else if (c == '[') {
      // structured type / tuple

      if (auto const pos_end_list =
              dict.find(']', pos_start_desc + desc.size());
          pos_end_list == std::string_view::npos) {
        throw std::runtime_error("invalid header: malformed list in 'descr'");
      } else {
        auto tuples_begin = std::cregex_iterator(
            dict.begin() + pos_start_desc + desc.size(),
            dict.begin() + pos_end_list, dtype_tuple_regex);
        auto tuples_end = std::cregex_iterator();

        for (std::cregex_iterator it = tuples_begin; it != tuples_end; ++it) {
          auto&& match = *it;
          labels.emplace_back(match[1].str());

          if (match[2].str() == ">") {
            throw std::runtime_error("parse_npy_header: data stored in "
                                     "big-endian format (not supported)");
          }

          data_types.push_back(*(match[3].first));
          word_sizes.push_back(std::stoi(match[4].str()));
        }
      }
    } else {
      throw std::runtime_error("invalid header: malformed 'descr'");
    }
  }
}

void cnpypp::parse_zip_footer(std::istream& fs, uint16_t& nrecs,
                              uint32_t& global_header_size,
                              uint32_t& global_header_offset) {
  std::array<uint8_t, 22> footer;
  fs.seekg(-22, std::ios_base::end);
  fs.read(reinterpret_cast<char*>(&footer[0]), sizeof(char) * 22);

  [[maybe_unused]] uint16_t disk_no, disk_start, nrecs_on_disk, comment_len;
  disk_no =
      boost::endian::endian_load<boost::uint16_t, 2,
                                 boost::endian::order::little>(&footer[4]);
  disk_start =
      boost::endian::endian_load<boost::uint16_t, 2,
                                 boost::endian::order::little>(&footer[6]);
  nrecs_on_disk =
      boost::endian::endian_load<boost::uint16_t, 2,
                                 boost::endian::order::little>(&footer[8]);
  nrecs = boost::endian::endian_load<boost::uint16_t, 2,
                                     boost::endian::order::little>(&footer[10]);
  global_header_size =
      boost::endian::endian_load<boost::uint32_t, 4,
                                 boost::endian::order::little>(&footer[12]);
  global_header_offset =
      boost::endian::endian_load<boost::uint32_t, 4,
                                 boost::endian::order::little>(&footer[16]);
  comment_len =
      boost::endian::endian_load<boost::uint16_t, 2,
                                 boost::endian::order::little>(&footer[20]);

  if (disk_no != 0 || disk_start != 0 || nrecs_on_disk != nrecs ||
      comment_len != 0) {
    throw std::runtime_error("parse_zip_footer: unexpected data");
  }
}

cnpypp::NpyArray load_the_npy_file(std::istream& fs) {
  std::vector<size_t> word_sizes, shape;
  std::vector<char> data_types;
  std::vector<std::string> labels;
  cnpypp::MemoryOrder memory_order;

  cnpypp::parse_npy_header(fs, word_sizes, data_types, labels, shape,
                           memory_order);

  cnpypp::NpyArray arr{std::move(shape), std::move(word_sizes),
                       std::move(labels), memory_order};
  fs.read(arr.data<char>(), arr.num_bytes());
  return arr;
}

cnpypp::NpyArray load_the_npz_array(std::istream& fs, uint32_t compr_bytes,
                                    uint32_t uncompr_bytes) {

  //  std::vector<std::istream::char_type> buffer_compr(compr_bytes);
  auto buffer_compr = std::make_unique<std::istream::char_type[]>(compr_bytes);
  auto buffer_uncompr =
      std::make_unique<std::istream::char_type[]>(uncompr_bytes);

  //  std::vector<std::istream::char_type> buffer_uncompr(uncompr_bytes);
  fs.read(&buffer_compr[0], compr_bytes);

  [[maybe_unused]] int err;
  z_stream d_stream;

  d_stream.zalloc = Z_NULL;
  d_stream.zfree = Z_NULL;
  d_stream.opaque = Z_NULL;
  d_stream.avail_in = 0;
  d_stream.next_in = Z_NULL;
  err = inflateInit2(&d_stream, -MAX_WBITS);

  d_stream.avail_in = compr_bytes;
  d_stream.next_in = reinterpret_cast<uint8_t*>(&buffer_compr[0]);
  d_stream.avail_out = uncompr_bytes;
  d_stream.next_out = reinterpret_cast<uint8_t*>(&buffer_uncompr[0]);

  err = inflate(&d_stream, Z_FINISH);
  err = inflateEnd(&d_stream);

  std::vector<size_t> shape, word_sizes;
  std::vector<char> data_types; // filled but not used
  std::vector<std::string> labels;
  cnpypp::MemoryOrder memory_order;
  cnpypp::parse_npy_header(&buffer_uncompr[0], word_sizes, data_types, labels,
                           shape, memory_order);

  cnpypp::NpyArray array(shape, std::move(word_sizes), std::move(labels),
                         memory_order);

  size_t const offset = uncompr_bytes - array.num_bytes();
  memcpy(array.data<unsigned char>(), &buffer_uncompr[0] + offset,
         array.num_bytes());

  return array;
}

cnpypp::npz_t cnpypp::npz_load(std::string const& fname) {
  std::ifstream fs{fname, std::ios::binary};

  if (!fs) {
    throw std::runtime_error("npz_load: Error! Unable to open file " + fname +
                             "!");
  }

  cnpypp::npz_t arrays;

  while (1) {
    std::vector<unsigned char> local_header(30);
    fs.read(reinterpret_cast<char*>(&local_header[0]), 30);

    // if we've reached the global header, stop reading
    if (local_header[2] != 0x03 || local_header[3] != 0x04)
      break;

    // read in the variable name
    uint16_t name_len = boost::endian::endian_load<
        boost::uint16_t, 2, boost::endian::order::little>(&local_header[26]);
    std::string varname(name_len, ' ');
    fs.read(&varname[0], sizeof(char) * name_len);

    // erase the lagging .npy
    varname.erase(varname.end() - 4, varname.end());

    // read in the extra field
    uint16_t extra_field_len = boost::endian::endian_load<
        boost::uint16_t, 2, boost::endian::order::little>(&local_header[28]);
    if (extra_field_len > 0) {
      std::vector<char> buff(extra_field_len);
      fs.read(&buff[0], extra_field_len);
    }

    uint16_t const compr_method = boost::endian::endian_load<
        boost::uint16_t, 2, boost::endian::order::little>(&local_header[0] + 8);
    uint32_t const compr_bytes =
        boost::endian::endian_load<boost::uint32_t, 4,
                                   boost::endian::order::little>(
            &local_header[0] + 18);
    uint32_t const uncompr_bytes =
        boost::endian::endian_load<boost::uint32_t, 4,
                                   boost::endian::order::little>(
            &local_header[0] + 22);

    if (compr_method == 0) {
      arrays.emplace(varname, load_the_npy_file(fs));
    } else {
      arrays.emplace(varname,
                     load_the_npz_array(fs, compr_bytes, uncompr_bytes));
    }
  }

  return arrays;
}

cnpypp::NpyArray cnpypp::npz_load(std::string const& fname,
                                  std::string const& varname) {
  std::ifstream fs(fname, std::ios::binary);

  if (!fs)
    throw std::runtime_error("npz_load: Unable to open file " + fname);

  while (1) {
    std::vector<unsigned char> local_header(30);
    fs.read(reinterpret_cast<char*>(&local_header[0]), sizeof(char) * 30);

    // if we've reached the global header, stop reading
    if (local_header[2] != 0x03 || local_header[3] != 0x04)
      break;

    // read in the variable name
    uint16_t const name_len = boost::endian::endian_load<
        boost::uint16_t, 2, boost::endian::order::little>(&local_header[26]);
    std::string vname(name_len, ' ');
    fs.read(&vname[0], sizeof(char) * name_len);
    vname.erase(vname.end() - 4, vname.end()); // erase the lagging .npy

    // read in the extra field
    uint16_t const extra_field_len = boost::endian::endian_load<
        boost::uint16_t, 2, boost::endian::order::little>(&local_header[28]);
    fs.seekg(extra_field_len, std::ios_base::cur); // skip past the extra field

    uint16_t const compr_method = boost::endian::endian_load<
        boost::uint16_t, 2, boost::endian::order::little>(&local_header[0] + 8);
    uint32_t const compr_bytes =
        boost::endian::endian_load<boost::uint32_t, 4,
                                   boost::endian::order::little>(
            &local_header[0] + 18);
    uint32_t const uncompr_bytes =
        boost::endian::endian_load<boost::uint32_t, 4,
                                   boost::endian::order::little>(
            &local_header[0] + 22);

    if (vname == varname) {
      return (compr_method == 0)
                 ? load_the_npy_file(fs)
                 : load_the_npz_array(fs, compr_bytes, uncompr_bytes);
    } else {
      // skip past the data
      uint32_t const size = boost::endian::endian_load<
          boost::uint32_t, 4, boost::endian::order::little>(&local_header[22]);
      fs.seekg(size, std::ios_base::cur);
    }
  }

  // if we get here, we haven't found the variable in the file
  throw std::runtime_error("npz_load: Variable name " + varname +
                           " not found in " + fname);
}

cnpypp::NpyArray cnpypp::npy_load(std::string const& fname) {
  std::ifstream fs{fname, std::ios::binary};

  if (!fs)
    throw std::runtime_error("npy_load: Unable to open file " + fname);

  return load_the_npy_file(fs);
}

std::vector<char> cnpypp::create_npy_header(
    gsl::span<size_t const> const shape,
    gsl::span<std::string_view const> labels, gsl::span<char const> dtypes,
    gsl::span<size_t const> sizes, MemoryOrder memory_order) {
  std::vector<char> dict;
  append(dict, "{'descr': [");

  if (labels.size() != dtypes.size() || dtypes.size() != sizes.size() ||
      sizes.size() != labels.size()) {
    throw std::runtime_error(
        "create_npy_header: sizes of argument vectors not equal");
  }

  for (size_t i = 0; i < dtypes.size(); ++i) {
    auto const& label = labels[i];
    auto const& dtype = dtypes[i];
    auto const& size = sizes[i];

    append(dict, "('");
    append(dict, label);
    append(dict, "', '");
    dict.push_back(BigEndianTest());
    dict.push_back(dtype);
    append(dict, std::to_string(size));
    append(dict, "')");

    if (i + 1 != dtypes.size()) {
      append(dict, ", ");
    }
  }

  if (dtypes.size() == 1) {
    dict.push_back(',');
  }

  append(dict, "], 'fortran_order': ");
  append(dict, (memory_order == MemoryOrder::C) ? "False" : "True");
  append(dict, ", 'shape': (");
  append(dict, std::to_string(shape[0]));
  for (size_t i = 1; i < shape.size(); i++) {
    append(dict, ", ");
    append(dict, std::to_string(shape[i]));
  }
  if (shape.size() == 1) {
    append(dict, ",");
  }
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

std::vector<char> cnpypp::create_npy_header(gsl::span<size_t const> const shape,
                                            char dtype, int size,
                                            MemoryOrder memory_order) {
  std::vector<char> dict;
  append(dict, "{'descr': '");
  dict += BigEndianTest();
  dict.push_back(dtype);
  append(dict, std::to_string(size));
  append(dict, "', 'fortran_order': ");
  append(dict, (memory_order == MemoryOrder::C) ? "False" : "True");
  append(dict, ", 'shape': (");
  append(dict, std::to_string(shape[0]));
  for (size_t i = 1; i < shape.size(); i++) {
    append(dict, ", ");
    append(dict, std::to_string(shape[i]));
  }
  if (shape.size() == 1) {
    append(dict, ",");
  }
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

// for C compatibility
int cnpypp_npy_save(char const* fname, cnpypp_data_type dtype,
                    void const* start, size_t const* shape, size_t rank,
                    char const* mode, enum cnpypp_memory_order memory_order) {
  int retval = 0;
  try {
    std::string const filename = fname;
    std::vector<size_t> shapeVec{};
    shapeVec.reserve(rank);
    std::copy_n(shape, rank, std::back_inserter(shapeVec));

    switch (dtype) {
    case cnpypp_int8:
      cnpypp::npy_save(filename, reinterpret_cast<int8_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint8:
      cnpypp::npy_save(filename, reinterpret_cast<uint8_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_int16:
      cnpypp::npy_save(filename, reinterpret_cast<int16_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint16:
      cnpypp::npy_save(filename, reinterpret_cast<uint16_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_int32:
      cnpypp::npy_save(filename, reinterpret_cast<int32_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint32:
      cnpypp::npy_save(filename, reinterpret_cast<uint32_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_int64:
      cnpypp::npy_save(filename, reinterpret_cast<int64_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint64:
      cnpypp::npy_save(filename, reinterpret_cast<uint64_t const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_float32:
      cnpypp::npy_save(filename, reinterpret_cast<float const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_float64:
      cnpypp::npy_save(filename, reinterpret_cast<double const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_float128:
      cnpypp::npy_save(filename, reinterpret_cast<long double const*>(start),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    default:
      std::cerr << "npy_save: unknown type argument" << std::endl;
    }
  } catch (...) {
    retval = -1;
  }

  return retval;
}

int cnpypp_npz_save(char const* zipname, char const* filename,
                    enum cnpypp_data_type dtype, void const* data,
                    size_t const* shape, size_t rank, char const* mode,
                    enum cnpypp_memory_order memory_order) {
  int retval = 0;
  try {
    std::vector<size_t> shapeVec{};
    shapeVec.reserve(rank);
    std::copy_n(shape, rank, std::back_inserter(shapeVec));

    switch (dtype) {
    case cnpypp_int8:
      cnpypp::npz_save(zipname, filename, reinterpret_cast<int8_t const*>(data),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint8:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<uint8_t const*>(data), shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_int16:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<int16_t const*>(data), shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint16:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<uint16_t const*>(data), shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_int32:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<int32_t const*>(data), shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint32:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<uint32_t const*>(data), shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_int64:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<int64_t const*>(data), shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_uint64:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<uint64_t const*>(data), shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_float32:
      cnpypp::npz_save(zipname, filename, reinterpret_cast<float const*>(data),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_float64:
      cnpypp::npz_save(zipname, filename, reinterpret_cast<double const*>(data),
                       shapeVec, mode,
                       static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    case cnpypp_float128:
      cnpypp::npz_save(zipname, filename,
                       reinterpret_cast<long double const*>(data), shapeVec,
                       mode, static_cast<cnpypp::MemoryOrder>(memory_order));
      break;
    default:
      std::cerr << "npz_save: unknown type argument" << std::endl;
    }
  } catch (...) {
    retval = -1;
  }

  return retval;
}

cnpypp_npyarray_handle* cnpypp_load_npyarray(char const* fname) {
  cnpypp::NpyArray* arr = nullptr;

  try {
    arr = new cnpypp::NpyArray(cnpypp::npy_load(fname));
  } catch (...) {
  }

  return reinterpret_cast<cnpypp_npyarray_handle*>(arr);
}

void cnpypp_free_npyarray(cnpypp_npyarray_handle* npyarr) {
  delete reinterpret_cast<cnpypp::NpyArray*>(npyarr);
}

void const* cnpypp_npyarray_get_data(cnpypp_npyarray_handle const* npyarr) {
  auto const& array = *reinterpret_cast<cnpypp::NpyArray const*>(npyarr);
  return array.data<void>();
}

size_t const* cnpypp_npyarray_get_shape(cnpypp_npyarray_handle const* npyarr,
                                        size_t* rank) {
  auto const& array = *reinterpret_cast<cnpypp::NpyArray const*>(npyarr);

  if (rank != nullptr) {
    *rank = array.getShape().size();
  }

  return array.getShape().data();
}

enum cnpypp_memory_order
cnpypp_npyarray_get_memory_order(cnpypp_npyarray_handle const* npyarr) {
  auto const& array = *reinterpret_cast<cnpypp::NpyArray const*>(npyarr);
  return (array.getMemoryOrder() == cnpypp::MemoryOrder::Fortran)
             ? cnpypp_memory_order_fortran
             : cnpypp_memory_order_c;
}
