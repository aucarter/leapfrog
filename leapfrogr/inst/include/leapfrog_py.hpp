#pragma once

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <vector>
#include <sstream>

#include "leapfrog.hpp"
#include "generated/py_interface/py_adapters.hpp"

template<typename ModelVariant>
using LeapfrogPy = leapfrog::Leapfrog<leapfrog::Py, double, ModelVariant>;

auto get_opts_py(
  const nb::dict parameters,
  const std::vector<int> output_years
) {
  const int t_art_start = nb::cast<int>(parameters["t_ART_start"]);
  const int hts_per_year = nb::cast<int>(parameters["hts_per_year"]);
  const int proj_start_year = nb::cast<int>(parameters["projection_start_year"]);
  const std::string projection_period = nb::cast<std::string>(parameters["projection_period"]);
  return leapfrog::get_opts<double>(
    hts_per_year, t_art_start, projection_period, proj_start_year, output_years
  );
}

template<typename ModelVariant>
auto build_output_py(
  typename LeapfrogPy<ModelVariant>::OutputState output_state,
  const std::vector<int> output_years
) {
  using LF = LeapfrogPy<ModelVariant>;
  nb::dict ret;
  LF::Cfg::build_output(0, output_state, ret, output_years.size());
  return ret;
}

template<typename ModelVariant>
auto build_output_py(
  typename LeapfrogPy<ModelVariant>::State state
) {
  using LF = LeapfrogPy<ModelVariant>;
  nb::dict ret;
  LF::Cfg::build_output_single_year(0, state, ret);
  return ret;
}

template<typename ModelVariant>
nb::dict simulate_model(
  const nb::dict parameters,
  const std::vector<int> output_years
) {
  using LF = LeapfrogPy<ModelVariant>;
  const auto opts = get_opts_py(parameters, output_years);
  const auto pars = LF::Cfg::get_pars(parameters, opts);
  auto state = LF::run_model(pars, opts, output_years);
  return build_output_py<ModelVariant>(state, output_years);
}

template<typename ModelVariant>
nb::dict simulate_model(
  const nb::dict parameters,
  const std::vector<int> output_years,
  nb::dict initial_state_data,
  int simulation_start_year
) {
  using LF = LeapfrogPy<ModelVariant>;
  const auto opts = get_opts_py(parameters, output_years);
  const auto pars = LF::Cfg::get_pars(parameters, opts);
  typename LF::State initial_state = LF::Cfg::get_initial_state(initial_state_data);
  auto state = LF::run_model_from_state(pars, opts, initial_state, simulation_start_year, output_years);
  return build_output_py<ModelVariant>(state, output_years);
}

template<typename ModelVariant>
nb::dict simulate_model(
  const nb::dict parameters,
  nb::dict initial_state_data,
  int simulation_start_year
) {
  using LF = LeapfrogPy<ModelVariant>;
  const auto opts = get_opts_py(parameters, { simulation_start_year + 1 });
  const auto pars = LF::Cfg::get_pars(parameters, opts);
  typename LF::State initial_state = LF::Cfg::get_initial_state(initial_state_data);
  auto state = LF::run_model_single_year(pars, opts, initial_state, simulation_start_year);
  return build_output_py<ModelVariant>(state);
}
