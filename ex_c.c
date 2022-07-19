#include <cnpy++.h>
#include <stdlib.h>

int main() {
  double const data[] = {1.2, 3.4, 5.6, 7.8};
  size_t const shape[] = {sizeof(data) / sizeof(data[0])};
  npy_save("data_from_c.npy", cnpypp_float64, &data, shape, 1, "w",
           cnpypp_memory_order_fortran);

  npy_save_1d("data_from_c2.npy", cnpypp_float64, &data, shape[0], "w");

  return EXIT_SUCCESS;
}
