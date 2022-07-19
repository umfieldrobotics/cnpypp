// Copyright (C) 2011  Carl Rogers, 2020-2022 Maximilian Reininghaus
// Released under MIT License
// license available in LICENSE file, or at
// http://www.opensource.org/licenses/mit-license.php

#include "cnpy++.hpp"
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

char cnpypp::BigEndianTest() {
  int32_t const x = 1;
  static_assert(sizeof(x) > 1);

  return (((char*)&x)[0]) ? '<' : '>';
}

char cnpypp::map_type(const std::type_info& t) {
  if (t == typeid(float))
    return 'f';
  if (t == typeid(double))
    return 'f';
  if (t == typeid(long double))
    return 'f';

  if (t == typeid(int))
    return 'i';
  if (t == typeid(char))
    return 'i';
  if (t == typeid(short))
    return 'i';
  if (t == typeid(long))
    return 'i';
  if (t == typeid(long long))
    return 'i';

  if (t == typeid(unsigned char))
    return 'u';
  if (t == typeid(unsigned short))
    return 'u';
  if (t == typeid(unsigned long))
    return 'u';
  if (t == typeid(unsigned long long))
    return 'u';
  if (t == typeid(unsigned int))
    return 'u';

  if (t == typeid(bool))
    return 'b';

  if (t == typeid(std::complex<float>))
    return 'c';
  if (t == typeid(std::complex<double>))
    return 'c';
  if (t == typeid(std::complex<long double>))
    return 'c';

  else {
    std::cerr << "cannot map " << std::quoted(t.name()) << "\n";
    return '?';
  }
}

std::vector<char>& cnpypp::append(std::vector<char>& vec,
                                  std::string_view view) {
  vec.insert(vec.end(), view.begin(), view.end());
  return vec;
}

static std::regex const num_regex("[0-9][0-9]*");

void cnpypp::parse_npy_header(std::istream::char_type* buffer,
                              size_t& word_size, std::vector<size_t>& shape,
                              cnpypp::MemoryOrder& memory_order) {
  // std::string magic_string(buffer,6);
  uint8_t const major_version = *reinterpret_cast<uint8_t*>(buffer + 6);
  uint8_t const minor_version = *reinterpret_cast<uint8_t*>(buffer + 7);
  uint16_t const header_len = *reinterpret_cast<uint16_t*>(buffer + 8);
  std::string header(reinterpret_cast<char*>(buffer + 9), header_len);

  if (!(major_version == 1 && minor_version == 0)) {
    throw std::runtime_exception("parse_npy_header: version not supported");
  }

  size_t loc1, loc2;

  // fortran order
  loc1 = header.find("fortran_order") + 16;
  memory_order =
      (header.substr(loc1, 4) == "True" ? cnpypp::MemoryOrder::Fortran
                                        : cnpypp::MemoryOrder::C);

  // shape
  loc1 = header.find("(");
  loc2 = header.find(")");

  std::smatch sm;
  shape.clear();

  std::string str_shape = header.substr(loc1 + 1, loc2 - loc1 - 1);
  while (std::regex_search(str_shape, sm, num_regex)) {
    shape.push_back(std::stoi(sm[0].str()));
    str_shape = sm.suffix().str();
  }

  // endian, word size, data type
  // byte order code | stands for not applicable.
  // not sure when this applies except for byte array
  loc1 = header.find("descr") + 9;
  bool const littleEndian = header[loc1] == '<' || header[loc1] == '|';

  if (!littleEndian) {
    throw std::runtime_error(
        "parse_npy_header: data stored in big-endian (not supported)");
  }

  // char type = header[loc1+1];
  // assert(type == map_type(T));

  std::string str_ws = header.substr(loc1 + 2);
  loc2 = str_ws.find("'");
  word_size = atoi(str_ws.substr(0, loc2).c_str());
}

void cnpypp::parse_npy_header(std::istream& fs, size_t& word_size,
                              std::vector<size_t>& shape,
                              cnpypp::MemoryOrder& memory_order) {
  fs.seekg(11, std::ios_base::cur);

  auto const pos = fs.tellg();
  char buf[20];
  fs.read(buf, 11);

  fs.seekg(pos, std::ios_base::beg);

  std::string header;
  std::getline(fs, header);

  size_t loc1, loc2;

  // fortran order
  loc1 = header.find("fortran_order");
  if (loc1 == std::string::npos)
    throw std::runtime_error(
        "parse_npy_header: failed to find header keyword: 'fortran_order'");
  loc1 += 16;
  memory_order = (header.substr(loc1, 4) == "True")
                     ? cnpypp::MemoryOrder::Fortran
                     : cnpypp::MemoryOrder::C;

  // shape
  loc1 = header.find("(");
  loc2 = header.find(")");
  if (loc1 == std::string::npos || loc2 == std::string::npos)
    throw std::runtime_error(
        "parse_npy_header: failed to find header keyword: '(' or ')'");

  std::smatch sm;
  shape.clear();

  std::string str_shape = header.substr(loc1 + 1, loc2 - loc1 - 1);
  while (std::regex_search(str_shape, sm, num_regex)) {
    shape.push_back(std::stoi(sm[0].str()));
    str_shape = sm.suffix().str();
  }

  // endian, word size, data type
  // byte order code | stands for not applicable.
  // not sure when this applies except for byte array
  loc1 = header.find("descr");
  if (loc1 == std::string::npos)
    throw std::runtime_error(
        "parse_npy_header: failed to find header keyword: 'descr'");
  loc1 += 9;
  bool const littleEndian = header[loc1] == '<' || header[loc1] == '|';
  if (!littleEndian) {
    throw std::runtime_error(
        "parse_npy_header: data stored in big-endian (not supported)");
  }

  // char type = header[loc1+1];
  // assert(type == map_type(T));

  std::string str_ws = header.substr(loc1 + 2);
  loc2 = str_ws.find("'");
  word_size = atoi(str_ws.substr(0, loc2).c_str());
}

void cnpypp::parse_zip_footer(std::istream& fs, uint16_t& nrecs,
                              size_t& global_header_size,
                              size_t& global_header_offset) {
  std::vector<char> footer(22);
  fs.seekg(-22, std::ios_base::end);
  fs.read(&footer[0], sizeof(char) * 22);

  [[maybe_unused]] uint16_t disk_no, disk_start, nrecs_on_disk, comment_len;
  disk_no = *(uint16_t*)&footer[4];
  disk_start = *(uint16_t*)&footer[6];
  nrecs_on_disk = *(uint16_t*)&footer[8];
  nrecs = *(uint16_t*)&footer[10];
  global_header_size = *(uint32_t*)&footer[12];
  global_header_offset = *(uint32_t*)&footer[16];
  comment_len = *(uint16_t*)&footer[20];

  if (disk_no != 0 || disk_start != 0 || nrecs_on_disk != nrecs ||
      comment_len != 0) {
    throw std::runtime_error("parse_zip_footer: unexpected data");
  }
}

cnpypp::NpyArray load_the_npy_file(std::istream& fs) {
  std::vector<size_t> shape;
  size_t word_size;
  cnpypp::MemoryOrder memory_order;
  cnpypp::parse_npy_header(fs, word_size, shape, memory_order);

  cnpypp::NpyArray arr(shape, word_size, memory_order);
  fs.read(arr.data<char>(), arr.num_bytes());
  return arr;
}

cnpypp::NpyArray load_the_npz_array(std::istream& fs, uint32_t compr_bytes,
                                    uint32_t uncompr_bytes) {

  std::vector<std::istream::char_type> buffer_compr(compr_bytes);
  std::vector<std::istream::char_type> buffer_uncompr(uncompr_bytes);
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

  std::vector<size_t> shape;
  size_t word_size;
  cnpypp::MemoryOrder memory_order;
  cnpypp::parse_npy_header(&buffer_uncompr[0], word_size, shape, memory_order);

  cnpypp::NpyArray array(shape, word_size, memory_order);

  size_t offset = uncompr_bytes - array.num_bytes();
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
    std::vector<std::ifstream::char_type> local_header(30);
    fs.read(&local_header[0], 30);

    // if we've reached the global header, stop reading
    if (local_header[2] != 0x03 || local_header[3] != 0x04)
      break;

    // read in the variable name
    uint16_t name_len = *(uint16_t*)&local_header[26];
    std::string varname(name_len, ' ');
    fs.read(&varname[0], sizeof(char) * name_len);

    // erase the lagging .npy
    varname.erase(varname.end() - 4, varname.end());

    // read in the extra field
    uint16_t extra_field_len = *(uint16_t*)&local_header[28];
    if (extra_field_len > 0) {
      std::vector<char> buff(extra_field_len);
      fs.read(&buff[0], extra_field_len);
    }

    uint16_t compr_method = *reinterpret_cast<uint16_t*>(&local_header[0] + 8);
    uint32_t compr_bytes = *reinterpret_cast<uint32_t*>(&local_header[0] + 18);
    uint32_t uncompr_bytes =
        *reinterpret_cast<uint32_t*>(&local_header[0] + 22);

    if (compr_method == 0) {
      arrays.emplace(varname, std::move(load_the_npy_file(fs)));
    } else {
      arrays.emplace(varname, std::move(load_the_npz_array(fs, compr_bytes,
                                                           uncompr_bytes)));
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
    std::vector<char> local_header(30);
    fs.read(&local_header[0], sizeof(char) * 30);

    // if we've reached the global header, stop reading
    if (local_header[2] != 0x03 || local_header[3] != 0x04)
      break;

    // read in the variable name
    uint16_t name_len = *(uint16_t*)&local_header[26];
    std::string vname(name_len, ' ');
    fs.read(&vname[0], sizeof(char) * name_len);
    vname.erase(vname.end() - 4, vname.end()); // erase the lagging .npy

    // read in the extra field
    uint16_t extra_field_len = *(uint16_t*)&local_header[28];
    fs.seekg(extra_field_len, std::ios_base::cur); // skip past the extra field

    uint16_t compr_method = *reinterpret_cast<uint16_t*>(&local_header[0] + 8);
    uint32_t compr_bytes = *reinterpret_cast<uint32_t*>(&local_header[0] + 18);
    uint32_t uncompr_bytes =
        *reinterpret_cast<uint32_t*>(&local_header[0] + 22);

    if (vname == varname) {
      NpyArray array = (compr_method == 0)
                           ? load_the_npy_file(fs)
                           : load_the_npz_array(fs, compr_bytes, uncompr_bytes);
      return array;
    } else {
      // skip past the data
      uint32_t size = *(uint32_t*)&local_header[22];
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

  NpyArray arr = load_the_npy_file(fs);

  return arr;
}

// for C compatibility
int cnpypp_npy_save(char const* fname, cnpypp_data_type dtype,
                    void const* start, size_t const* shape, size_t rank,
                    char const* mode, enum cnpypp_memory_order memory_order) {
  std::string const filename = fname;
  std::vector<size_t> shapeVec{};
  shapeVec.reserve(rank);
  std::copy_n(shape, rank, std::back_inserter(shapeVec));

  switch (dtype) {
  case cnpypp_int8:
    cnpypp::npy_save(filename, reinterpret_cast<int8_t const*>(start), shapeVec,
                     mode, static_cast<cnpypp::MemoryOrder>(memory_order));
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
    cnpypp::npy_save(filename, reinterpret_cast<float const*>(start), shapeVec,
                     mode, static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_float64:
    cnpypp::npy_save(filename, reinterpret_cast<double const*>(start), shapeVec,
                     mode, static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_float128:
    cnpypp::npy_save(filename, reinterpret_cast<long double const*>(start),
                     shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  default:
    std::cerr << "npy_save: unknown type argument" << std::endl;
  }

  return 0;
}

int cnpypp_npz_save(char const* zipname, char const* filename,
                    enum cnpypp_data_type dtype, void const* data,
                    size_t const* shape, size_t rank, char const* mode,
                    enum cnpypp_memory_order memory_order) {
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
    cnpypp::npz_save(zipname, filename, reinterpret_cast<uint8_t const*>(data),
                     shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_int16:
    cnpypp::npz_save(zipname, filename, reinterpret_cast<int16_t const*>(data),
                     shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_uint16:
    cnpypp::npz_save(zipname, filename, reinterpret_cast<uint16_t const*>(data),
                     shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_int32:
    cnpypp::npz_save(zipname, filename, reinterpret_cast<int32_t const*>(data),
                     shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_uint32:
    cnpypp::npz_save(zipname, filename, reinterpret_cast<uint32_t const*>(data),
                     shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_int64:
    cnpypp::npz_save(zipname, filename, reinterpret_cast<int64_t const*>(data),
                     shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  case cnpypp_uint64:
    cnpypp::npz_save(zipname, filename, reinterpret_cast<uint64_t const*>(data),
                     shapeVec, mode,
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
                     reinterpret_cast<long double const*>(data), shapeVec, mode,
                     static_cast<cnpypp::MemoryOrder>(memory_order));
    break;
  default:
    std::cerr << "npz_save: unknown type argument" << std::endl;
  }

  return 0;
}
