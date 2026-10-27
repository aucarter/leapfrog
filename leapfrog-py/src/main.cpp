#include "../../leapfrog-core/include/leapfrog_py.hpp"

std::vector<std::string> list_model_configurations() {
  return std::vector<std::string>{
    "DemographicProjection",
    "HivFullAgeStratification",
    "HivFullAgeStratificationEntrants",
    "HivCoarseAgeStratification",
    "HivCoarseAgeStratificationEntrants",
    "ChildModel",
    "Spectrum",
    "SpectrumPopAdjust"
  };
}

template<typename ...Args>
auto sim_model(const std::string configuration, Args&&... args) {
  if (configuration == "DemographicProjection") {
    return simulate_model<leapfrog::DemographicProjection>(std::forward<Args>(args)...);
  } else if (configuration == "HivFullAgeStratification") {
    return simulate_model<leapfrog::HivFullAgeStratification>(std::forward<Args>(args)...);
  } else if (configuration == "HivFullAgeStratificationEntrants") {
    return simulate_model<leapfrog::HivFullAgeStratificationEntrants>(std::forward<Args>(args)...);
  } else if (configuration == "HivCoarseAgeStratification") {
    return simulate_model<leapfrog::HivCoarseAgeStratification>(std::forward<Args>(args)...);
  } else if (configuration == "HivCoarseAgeStratificationEntrants") {
    return simulate_model<leapfrog::HivCoarseAgeStratificationEntrants>(std::forward<Args>(args)...);
  } else if (configuration == "ChildModel") {
    return simulate_model<leapfrog::ChildModel>(std::forward<Args>(args)...);
  } else if (configuration == "Spectrum") {
    return simulate_model<leapfrog::Spectrum>(std::forward<Args>(args)...);
  } else if (configuration == "SpectrumPopAdjust") {
    return simulate_model<leapfrog::SpectrumPopAdjust>(std::forward<Args>(args)...);
  } else {
    const auto available_variants = list_model_configurations();
    std::ostringstream oss;
    oss << "Invalid configuration: '" << configuration
        << "'. It must be one of: ";
    for (size_t i = 0; i < available_variants.size(); ++i) {
      oss << "'" << available_variants[i] << "'";
      if (i != available_variants.size() - 1) {
        oss << ", ";
      } else {
        oss << ".";
      }
    }
    throw std::runtime_error(oss.str());
  }
};


nb::dict run_base_model(
  const nb::dict parameters,
  const std::string configuration,
  const std::vector<int> output_years
) {
  return sim_model(configuration, parameters, output_years);
}

nb::dict run_base_model_from_state(
  const nb::dict parameters,
  const std::string configuration,
  const nb::dict initial_state,
  int simulation_start_year,
  const std::vector<int> output_years
) {
  return sim_model(configuration, parameters, output_years, initial_state, simulation_start_year);
}

nb::dict run_base_model_single_year(
  const nb::dict parameters,
  const std::string configuration,
  const nb::dict initial_state,
  int simulation_start_year
) {
  return sim_model(configuration, parameters, initial_state, simulation_start_year);
}


nb::dict get_leapfrog_ss(const std::string configuration) {
  if (configuration == "DemographicProjection") {
    return leapfrog::get_ss_py<leapfrog::DemographicProjection>();
  } else if (configuration == "HivFullAgeStratification") {
    return leapfrog::get_ss_py<leapfrog::HivFullAgeStratification>();
  } else if (configuration == "HivFullAgeStratificationEntrants") {
    return leapfrog::get_ss_py<leapfrog::HivFullAgeStratificationEntrants>();
  } else if (configuration == "HivCoarseAgeStratification") {
    return leapfrog::get_ss_py<leapfrog::HivCoarseAgeStratification>();
  } else if (configuration == "HivCoarseAgeStratificationEntrants") {
    return leapfrog::get_ss_py<leapfrog::HivCoarseAgeStratificationEntrants>();
  } else if (configuration == "ChildModel") {
    return leapfrog::get_ss_py<leapfrog::ChildModel>();
  } else if (configuration == "Spectrum") {
    return leapfrog::get_ss_py<leapfrog::Spectrum>();
  } else if (configuration == "SpectrumPopAdjust") {
    return leapfrog::get_ss_py<leapfrog::SpectrumPopAdjust>();
  } else {
    const auto available_variants = list_model_configurations();
    std::ostringstream oss;
    oss << "Invalid configuration: '" << configuration
        << "'. It must be one of: ";
    for (size_t i = 0; i < available_variants.size(); ++i) {
      oss << "'" << available_variants[i] << "'";
      if (i != available_variants.size() - 1) {
        oss << ", ";
      } else {
        oss << ".";
      }
    }
    throw std::runtime_error(oss.str());
  }
}

NB_MODULE(_core, m) {
  m.doc() = "Leapfrog python interface";

  m.def("run_base_model", &run_base_model, R"pbdoc(
      Run the leapfrog model.
  )pbdoc");
  m.def("run_base_model_from_state", &run_base_model_from_state, R"pbdoc(
      Run the leapfrog model from an initial state.
  )pbdoc");
  m.def("run_base_model_single_year", &run_base_model_single_year, R"pbdoc(
      Run the leapfrog model from an initial state for a single year.
  )pbdoc");
  m.def("get_leapfrog_ss_py", &get_leapfrog_ss, R"pbdoc(
    Get the state space dimensions for a specific model variant,
  )pbdoc");
}
