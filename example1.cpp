#include "cnpy.hpp"

#include <complex>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <numeric>
#include <map>
#include <string>

const int Nx = 2;
const int Ny = 4;
const int Nz = 8;

int main()
{
    auto const data = std::invoke([]() {
      //std::vector<std::complex<double>> data(Nx*Ny*Nz);
      //std::complex<double> c{1., 1.};
      //std::generate(data.begin(), data.end(), [&c]{ return c += std::complex<double>{1.,1.}; }); 
      std::vector<uint32_t> data(Nx*Ny*Nz);
      std::iota(data.begin(), data.end(), 1);
      return data;
    });

    //save it to file
    cnpy::npy_save("arr1.npy",&data[0],{Nz,Ny,Nx},"w");
    cnpy::npy_save("arr1-cpy.npy", data.cbegin(), {Nz,Ny,Nx},"w");

    return EXIT_SUCCESS;

    //load it into a new array
    cnpy::NpyArray const arr = cnpy::npy_load("arr1.npy");
    auto const* const loaded_data = arr.data<uint32_t>();
    
    //make sure the loaded data matches the saved data
    assert(arr.word_size == sizeof(std::complex<double>));
    assert(arr.shape.size() == 3 && arr.shape[0] == Nz && arr.shape[1] == Ny && arr.shape[2] == Nx);
    for(int i = 0; i < Nx*Ny*Nz;i++) assert(data[i] == loaded_data[i]);

    //append the same data to file
    //npy array on file now has shape (Nz+Nz,Ny,Nx)
    cnpy::npy_save("arr1.npy",&data[0],{Nz,Ny,Nx},"a");
    cnpy::npy_save("arr2.npy",&data[0],{Nz,Ny,Nx},"w");

    //now write to an npz file
    //non-array variables are treated as 1D arrays with 1 element
    double myVar1 = 1.2;
    char myVar2[] = "abcdefghijklmno";
    cnpy::npz_save("out.npz","myVar2", myVar2,{sizeof(myVar2)},"w"); //"w" overwrites any existing file

    return EXIT_SUCCESS;
    //cnpy::npz_save("out.npz","myVar2",&myVar2,{1},"a"); //"a" appends to the file we created above
    cnpy::npz_save("out.npz","arr1",&data[0],{Nz,Ny,Nx},"a"); //"a" appends to the file we created above

    //load a single var from the npz file
    cnpy::NpyArray arr2 = cnpy::npz_load("out.npz","arr1");

    //load the entire npz file
    cnpy::npz_t my_npz = cnpy::npz_load("out.npz");
    
    //check that the loaded myVar1 matches myVar1
    cnpy::NpyArray const& arr_mv1 = my_npz.find("myVar1")->second;
    double const* mv1 = arr_mv1.data<double>();
    assert(arr_mv1.shape.size() == 1 && arr_mv1.shape[0] == 1);
    assert(mv1[0] == myVar1);
}
