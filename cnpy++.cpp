// Copyright (C) 2011  Carl Rogers
// Released under MIT License
// license available in LICENSE file, or at
// http://www.opensource.org/licenses/mit-license.php

#include "cnpy++.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <regex>
#include <stdexcept>
#include <stdint.h>

char cnpypp::BigEndianTest() {
  int x = 1;
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

std::vector<char>& cnpypp::append(std::vector<char>& vec, std::string_view view) {
  vec.insert(vec.end(), view.begin(), view.end());
  return vec;
}

void cnpypp::parse_npy_header(std::istream::char_type* buffer, size_t& word_size,
                            std::vector<size_t>& shape, cnpypp::MemoryOrder& memory_order) {
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
  memory_order = (header.substr(loc1, 4) == "True" ? cnpypp::MemoryOrder::Fortran : cnpypp::MemoryOrder::C);

  // shape
  loc1 = header.find("(");
  loc2 = header.find(")");

  std::regex num_regex("[0-9][0-9]*");
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
  bool littleEndian =
      (header[loc1] == '<' || header[loc1] == '|' ? true : false);
  assert(littleEndian);

  // char type = header[loc1+1];
  // assert(type == map_type(T));

  std::string str_ws = header.substr(loc1 + 2);
  loc2 = str_ws.find("'");
  word_size = atoi(str_ws.substr(0, loc2).c_str());
}

void cnpypp::parse_npy_header(std::istream& fs, size_t& word_size,
                            std::vector<size_t>& shape, cnpypp::MemoryOrder& memory_order) {
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
  memory_order = (header.substr(loc1, 4) == "True") ? cnpypp::MemoryOrder::Fortran : cnpypp::MemoryOrder::C;

  // shape
  loc1 = header.find("(");
  loc2 = header.find(")");
  if (loc1 == std::string::npos || loc2 == std::string::npos)
    throw std::runtime_error(
        "parse_npy_header: failed to find header keyword: '(' or ')'");

  std::regex num_regex("[0-9][0-9]*");
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
  bool littleEndian =
      (header[loc1] == '<' || header[loc1] == '|' ? true : false);
  assert(littleEndian);

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

  uint16_t disk_no, disk_start, nrecs_on_disk, comment_len;
  disk_no = *(uint16_t*)&footer[4];
  disk_start = *(uint16_t*)&footer[6];
  nrecs_on_disk = *(uint16_t*)&footer[8];
  nrecs = *(uint16_t*)&footer[10];
  global_header_size = *(uint32_t*)&footer[12];
  global_header_offset = *(uint32_t*)&footer[16];
  comment_len = *(uint16_t*)&footer[20];

  assert(disk_no == 0);
  assert(disk_start == 0);
  assert(nrecs_on_disk == nrecs);
  assert(comment_len == 0);
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

  int err;
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
