#include <chrono>
#include <cstdlib>
#include <iostream>

#include <cnpy++.hpp>

#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

int main() {
  auto const i = ranges::views::ints(1, 1 << 22);
  auto const sq = i | ranges::views::transform([](auto x) { return x * x; });
  auto const trip =
      i | ranges::views::transform([](auto x) { return x * x * x; });

  auto const z = ranges::views::zip(i, sq, trip);

  std::array const shape{z.size()};

  {
    auto const begin = std::chrono::steady_clock::now();
    cnpypp::npz_save(
        "range_zip_data_zstd.npz", "struct", {"a", "b", "c"}, z.begin(),
        cnpypp::span<size_t const>{shape.data(), shape.size()}, "a",
        cnpypp::MemoryOrder::C, cnpypp::CompressionMethod::ZSTD);
    auto const end = std::chrono::steady_clock::now();
    std::cout << "ZSTD    "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                       begin)
                         .count() /
                     1e3
              << std::endl;
  }

  {
    auto const begin = std::chrono::steady_clock::now();
    cnpypp::npz_save(
        "range_zip_data_bzip2.npz", "struct", {"a", "b", "c"}, z.begin(),
        cnpypp::span<size_t const>{shape.data(), shape.size()}, "a",
        cnpypp::MemoryOrder::C, cnpypp::CompressionMethod::BZip2);
    auto const end = std::chrono::steady_clock::now();
    std::cout << "BZip2   "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                       begin)
                         .count() /
                     1e3
              << std::endl;
  }

  {
    auto const begin = std::chrono::steady_clock::now();
    cnpypp::npz_save(
        "range_zip_data_deflate.npz", "struct", {"a", "b", "c"}, z.begin(),
        cnpypp::span<size_t const>{shape.data(), shape.size()}, "a",
        cnpypp::MemoryOrder::C, cnpypp::CompressionMethod::Deflate);
    auto const end = std::chrono::steady_clock::now();
    std::cout << "Deflate "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                       begin)
                         .count() /
                     1e3
              << std::endl;
  }

  {
    auto const begin = std::chrono::steady_clock::now();
    cnpypp::npz_save(
        "range_zip_data_store.npz", "struct", {"a", "b", "c"}, z.begin(),
        cnpypp::span<size_t const>{shape.data(), shape.size()}, "a",
        cnpypp::MemoryOrder::C, cnpypp::CompressionMethod::Store);
    auto const end = std::chrono::steady_clock::now();
    std::cout << "Store   "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                       begin)
                         .count() /
                     1e3
              << std::endl;
  }
  return EXIT_SUCCESS;
}
