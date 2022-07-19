#include <cnpy++.h>

int npy_save_1d(char const* fname, enum cnpypp_data_type dtype,
                void const* start, size_t const num_elem, char const* mode) {
  size_t const shape[] = {num_elem};
  return npy_save(fname, dtype, start, shape, 1, mode, cnpypp_memory_order_c);
}

int npz_save_1d(char const* zipname, char const* fname,
                enum cnpypp_data_type dtype, void const* data, size_t num_elem,
                char const* mode) {
  size_t const shape[] = {num_elem};
  return npz_save(zipname, fname, dtype, data, shape, 1, mode,
                  cnpypp_memory_order_c);
}
