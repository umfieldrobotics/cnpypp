#ifndef _CNPYPP_H_
#define _CNPYPP_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum cnpypp_memory_order { cnpypp_memory_order_fortran = 0, cnpypp_memory_order_c = 1 };
enum cnpypp_data_type {
    cnpypp_int8 = 0,
    cnpypp_uint8 = 1,
    cnpypp_int16 = 2,
    cnpypp_uint16 = 3,
    cnpypp_int32 = 4,
    cnpypp_uint32 = 5,
    cnpypp_int64 = 6,
    cnpypp_uint64 = 7,
    cnpypp_float32 = 8,
    cnpypp_float64 = 9,
    cnpypp_float128 = 10
};

int npy_save(char const* fname, enum cnpypp_data_type, void const* start,
              size_t const* shape, size_t rank, char const* mode,
              enum cnpypp_memory_order);
int npy_save_1d(char const* fname, enum cnpypp_data_type, void const* start,
              size_t const num_elem, char const* mode);


#ifdef __cplusplus
}
#endif
#endif // _CNPYPP_H
