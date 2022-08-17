#include <cstdlib>

#include <cnpy++.hpp>

#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

int main() {
  auto const i = ranges::views::ints(1, 10);
  auto const sq = i | ranges::views::transform([](auto x) { return x * x; });
  auto const trip =
      i | ranges::views::transform([](auto x) { return x * x * x; });

  auto const z = ranges::views::zip(i, sq, trip);
  cnpypp::npy_save("range_zip_data.npy", {"a", "b", "c"}, z.begin(), {z.size()});

  return EXIT_SUCCESS;
}
