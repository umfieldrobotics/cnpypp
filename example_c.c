#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cnpy++.h>

int main() {
  double const data[] = {1.2, 3.4, 5.6, 7.8};
  size_t const shape[] = {sizeof(data) / sizeof(data[0])};
  npy_save("data_from_c.npy", cnpypp_float64, &data, shape, 1, "w",
           cnpypp_memory_order_fortran);

  npy_save_1d("data_from_c2.npy", cnpypp_float64, &data, shape[0], "w");

  char const* const str = "Hello";
  char const* const str2 = "World!";
  npz_save_1d("archive.npz", "str", cnpypp_uint8, str, strlen(str), "w");
  npz_save_1d("archive.npz", "str2", cnpypp_uint8, str2, strlen(str2), "a");

  npy_save_1d("string.npy", cnpypp_uint8, str, strlen(str), "w");
  npy_save_1d("string.npy", cnpypp_uint8, str2, strlen(str2), "a");

  return EXIT_SUCCESS;
}
