#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace leapfrog {
namespace internal {

using SSTypes = std::variant<
  int,
  double,
  std::vector<int>,
  std::vector<double>,
  std::vector<std::vector<double>>
>;

using SSMap = std::map<std::string, SSTypes>;

struct SSCollector {
    SSMap& out;

    void add(std::string_view name, int v) {
    out.insert_or_assign(std::string(name), v);
  }

  void add(std::string_view name, double v) {
    out.insert_or_assign(std::string(name), v);
  }

  template<size_t N>
  void add(std::string_view name, const std::array<int, N>& v) {
    out.insert_or_assign(std::string(name), std::vector<int>(v.begin(), v.end()));
  }

  template<size_t N>
  void add(std::string_view name, const std::array<double, N>& v) {
    out.insert_or_assign(std::string(name), std::vector<double>(v.begin(), v.end()));
  }

  template<size_t R, size_t C>
  void add(std::string_view name,
           const std::array<std::array<double, C>, R>& v) {
    std::vector<std::vector<double>> m;
    m.reserve(R);
    for (const auto& row : v) {
      m.emplace_back(row.begin(), row.end());
    }
    out.insert_or_assign(std::string(name), std::move(m));
  }
};

} // namespace internal
} // namespace leapfrog
