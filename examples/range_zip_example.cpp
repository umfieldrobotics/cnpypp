#include <cstdlib>

#include <cnpy++.hpp>

#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

int main() {
  auto const i = ranges::views::ints(1, 21);
  auto const sq = i | ranges::views::transform([](auto x) { return x * x; });
  auto const trip =
      i | ranges::views::transform([](auto x) { return x * x * x; });

  auto const z = ranges::views::zip(i, sq, trip);
  cnpypp::npy_save("range_zip_data.npy", {"a", "b", "c"}, z.begin(),
                   {z.size()});

  std::array const shape{z.size()};
  cnpypp::npz_save("range_zip_data.npz", "struct", {"a", "b", "c"}, z.begin(),
                   gsl::span{shape.data(), shape.size()});

  return EXIT_SUCCESS;
}
