# cnpy++

`cnpy++` is a C++17 library that allows to read and write NumPy data files (.npy and .npz).
It is designed in a way to integrate well into the modern C++ ecosystem and it provides features not available
in any similar C++/npy library.

Additionally, C bindings are provided for a limited, but most useful subset of the C++ functionality.

## Motivation
NumPy data files are a binary data format for serializing multi-dimenstional, potentially also nested, arrays.
Due to its simplicity, it is a convenient format for scientific computing to be used not only from within Python. 

## Installation:

The default installation directory is /usr/local.
To specify a different directory, add `-DCMAKE_INSTALL_PREFIX=/path/to/install/dir` to the cmake invocation in step 4.

1. get [cmake](www.cmake.org)
2. create a build directory, say $HOME/build
3. cd $HOME/build
4. cmake /path/to/cnpy++
5. make
6. make install

## Using:

`cnpy++` consists of a header part, `cnpy++.hpp`, which needs to be included in your source file,
and a compiled part, which can either be a shared or a static library.


### Manual compilation
If you build `cnpy++` with `cmake -DBUILD_SHARED_LIBS=ON`, you obtain a shared library, `libcnpy++.so`,
that you need to link to your executable. On Unix systems with g++ or clang++ compilers, you can run

```bash
g++ -o my_executable my_executable.cpp -L/path/to/install/dir -lcnpy++
```

This works analogously if you use the C bindings with a C compiler.

In case of a static `cnpy++` build, you need to provide the path to `libcnpy++.a`:
```bash
g++ -o my_executable my_executable.cpp /path/to/libcnpy++.a
```

### cmake-assisted compilation

TODO

## Documentation

### Writing data to .npy

To write data into a NPY file, use one of the overloads of `npy_save()`:

```c++
template <typename TConstInputIterator>
void npy_save(std::string const& fname, TConstInputIterator start,
              std::initializer_list<size_t> const shape,
              std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C)
```
This function writes data from an interator `start` into the file indicated by the filename `fname`.

The `shape` tuple describes the dimensions of the array, with the total number of elements given
by the product of all entries of `shape`.

The `mode` parameter can be either "w" or "a". With "w", a potentially existing file is overwritten.
With "a", data are appended if the file already exists. In that case, the data shape has to match the
shape in the existing file in all entries except the first.

The `memory_order` parameter indicates the memory order and can be either `MemoryOrder::C`, `MemoryOrder::Fortran`,
or their aliases `MemoryOrder::RowMajor` and `MemoryOrder::ColumnMajor`.

```c++
template <typename TConstInputIterator>
void npy_save(std::string const& fname, TConstInputIterator start,
              gsl::span<size_t const> const shape, std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C)
```
Use this overload if `shape` is an array, vector or alike. We are using `gsl::span` as C++17 implementation
of `std::span`, which is available only from C++20 on.

```c++
template <typename TForwardIterator>
void npy_save(std::string const& fname, TForwardIterator first,
              TForwardIterator last, std::string_view mode = "w")
```
This is an overload provided for convenience when the data to be written are available
via a pair of multiple-pass forward iterators. They are assumed to be one-dimensional.

```c++
template <typename T>
void npy_save(std::string const& fname, gsl::span<T const> data,
              std::string_view mode = "w")
```
This overload is provided for convenience when your data are in contiguous memory.

```c++
template <typename TTupleIterator>
void npy_save(std::string const& fname,
              std::vector<std::string_view> const& labels, TTupleIterator first,
              gsl::span<size_t const> const shape, std::string_view mode = "w",
              MemoryOrder memory_order = MemoryOrder::C)
```
With this overload, it is possible to write labeled "structured arrays" (in the terminology of NumPy).
The iterator must yield `std::tuple`s of values (e.g. `std::tuple<int, float>`) or references (e.g.
`std::tuple<int const&, float const&>`). The `label` vector must have a size equal to the number of
elements in the tuple. A potential use-case is to use a `zip_iterator` from the [range-v3](https://github.com/ericniebler/range-v3)
library. This way, you can serialize data in a structure-of-arrays layout as array-of-structures.
An example of this usage is provided in `examples/range_zip_example.cpp`.


# Description:

There are two functions for writing data: `npy_save` and `npz_save`.

There are 3 functions for reading:
- `npy_load` will load a .npy file. 
- `npz_load(fname)` will load a .npz and return a dictionary of NpyArray structues. 
- `npz_load(fname,varname)` will load and return the NpyArray for data varname from the specified .npz file.

The data structure for loaded data is below. 
Data is accessed via the `data<T>()`-method, which returns a pointer of the specified type (which must match the underlying datatype of the data). 
The array shape and word size are read from the npy header.

```c++
struct NpyArray {
    std::vector<size_t> shape;
    size_t word_size;
    template<typename T> T* data();
};
```

See [example1.cpp](example1.cpp) for examples of how to use the library. example1 will also be build during cmake installation.
