#include "H5Cpp.h"
#include "array/array.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <string_view>
#include <numeric>
#include <sstream>
#include <stdexcept>

#include "leapfrog.hpp"
#include "generated/cpp_interface/cpp_adapters.hpp"

namespace {

// Kept deliberately small: just the ModelVariants this binary actually
// needs for the cross-interface (R/Python/C++) equality check's
// shape-diverse scenarios (ticket 18) -- HivFullAgeStratification (the
// original default), HivCoarseAgeStratification (the coarse-age-groups
// scenario) and Spectrum (the PMTCT/child-params scenario, the smallest
// ModelVariant that actually exercises the child-model pars adapter).
// Mirrors the string-dispatch pattern leapfrog-py/src/main.cpp and
// leapfrogr/src/leapfrog.cpp already use for the same purpose.
std::vector<std::string> supported_configurations() {
  return {"HivFullAgeStratification", "HivCoarseAgeStratification", "Spectrum"};
}

template<typename ModelVariant>
void simulate_and_write(
  const std::filesystem::path& params_abs,
  std::filesystem::path output_file,
  const std::vector<int>& output_years,
  size_t n_runs
) {
  using LF = leapfrog::Leapfrog<leapfrog::Cpp, double, ModelVariant>;
  using OP = leapfrog::internal::OwnedParsMixed<double, ModelVariant>;

  // NOTE (pre-existing, not introduced by ticket 18, but now load-bearing
  // for 3 fixtures instead of 1): hts_per_year/t_ART_start/the projection
  // start year/period are hardcoded here rather than read from the params
  // file the way R's get_opts_r and Python's get_opts_py do from their
  // `parameters` list/dict. A runtime cross-check against the params file
  // was considered and deliberately not added: `hts_per_year` isn't
  // actually present as a stored scalar in any of this repo's real params
  // fixtures (process_pjnz() returns it as NULL, so R's save_datasets()
  // drops it), so reading it back would throw rather than validate
  // anything. Confirmed empirically (not just assumed) that all three of
  // this binary's supported configurations' real params fixtures share
  // the same actual t_ART_start/projection_start_year/projection_period --
  // see ticket 18's Comments for the values and how they were checked;
  // revisit if a future fixture for this binary ever has different ones.
  const auto opts = leapfrog::get_opts<double>(10, 30, std::string_view{"midyear"}, 1970, output_years);
  auto owned_pars = OP::parse_pars(params_abs, opts);
  const auto pars = LF::Cfg::get_pars(owned_pars);
  for (size_t i = 0; i < n_runs; ++i) {
    auto state = LF::run_model(pars, opts, output_years);
  }
  std::cout << "Fit complete" << std::endl;

  auto state = LF::run_model(pars, opts, output_years);

  const H5std_string FILE_NAME(output_file);
  H5::H5File file(FILE_NAME, H5F_ACC_TRUNC);
  file.close();

  LF::Cfg::build_output(0, state, output_file);
}

void run_configuration(
  const std::string& configuration,
  const std::filesystem::path& params_abs,
  const std::filesystem::path& output_file,
  const std::vector<int>& output_years,
  size_t n_runs
) {
  if (configuration == "HivFullAgeStratification") {
    simulate_and_write<leapfrog::HivFullAgeStratification>(params_abs, output_file, output_years, n_runs);
  } else if (configuration == "HivCoarseAgeStratification") {
    simulate_and_write<leapfrog::HivCoarseAgeStratification>(params_abs, output_file, output_years, n_runs);
  } else if (configuration == "Spectrum") {
    simulate_and_write<leapfrog::Spectrum>(params_abs, output_file, output_years, n_runs);
  } else {
    const auto available = supported_configurations();
    std::ostringstream oss;
    oss << "Invalid configuration: '" << configuration << "'. It must be one of: ";
    for (size_t i = 0; i < available.size(); ++i) {
      oss << "'" << available[i] << "'";
      if (i != available.size() - 1) {
        oss << ", ";
      } else {
        oss << ".";
      }
    }
    throw std::runtime_error(oss.str());
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 4) {
    std::cout <<
              "Usage: simulate_model <sim_years> <params_file> <output_dir> [configuration]"
              <<
              std::endl;
    return 1;
  }

  int sim_years = atoi(argv[1]);
  std::string params_file = argv[2];
  std::string output_dir = argv[3];
  std::string configuration = argc > 4 ? argv[4] : "HivFullAgeStratification";

  std::filesystem::path params_abs = std::filesystem::absolute(params_file);
  if (!std::filesystem::exists(params_abs)) {
    std::cout << "Params file '" << params_file << "' does not exist." << std::endl;
    return 1;
  }

  std::filesystem::path output_abs = std::filesystem::absolute(output_dir);
  if (!std::filesystem::exists(output_abs)) {
    if (std::filesystem::create_directory(output_abs)) {
      std::cout << "Created output directory '" << std::string{output_abs} << "'"
                << std::endl;
    } else {
      std::cout << "Failed to create output directory '" << std::string{output_abs} << "'"
                << std::endl;
    }
  } else {
    std::cout << "Writing to existing output directory " << std::string{output_abs} << "'"
              << std::endl;
  }

  if (sim_years > 61) {
    std::cout << "Running to max no of sim years: 61\n" << std::endl;
    sim_years = 61;
  }

  std::vector<int> output_years(sim_years);
  std::iota(output_years.begin(), output_years.end(), 1970);

  const char *n_runs_char = std::getenv("N_RUNS");
  size_t n_runs = 1;
  if (n_runs_char != nullptr) {
    // If we're profiling we want to get accurate info about where time is spent during the
    // main model fit. This runs so quickly though that just going through once won't sample enough
    // times for us to see. And it will sample from the tensor file serialization/deserialization more.
    // So we run the actual model fit multiple times when profiling so the sampler can actually pick
    // up the slow bits.
    n_runs = atoi(n_runs_char);
    std::cout << "Running model fit " << n_runs << " times" << std::endl;
  }

  std::filesystem::path output_file = output_abs / "output.h5";

  try {
    run_configuration(configuration, params_abs, output_file, output_years, n_runs);
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
    return 1;
  }

  return 0;
}
