#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>

#include "array/array.h"

struct NdaInfo;

template <class Array>
NdaInfo nda_capture(const Array& arr);

struct DpDebugInfo;

struct HvDebugInfo;

struct HaDebugInfo;

struct HcDebugInfo;

struct ModelDebugInfo;

template <class DpState, class IntermediateDp, class ParsDp>
  requires requires (const DpState& dp, const IntermediateDp& i_dp, const ParsDp& p_dp) {
    dp.p_totpop;
    i_dp.migration_rate;
    p_dp.total_fertility_rate;
  }
inline DpDebugInfo capture_dp(const DpState& dp, const IntermediateDp& i_dp,
  const ParsDp& p_dp);

template <class HvState, class IntermediateHv, class ParsHv>
  requires requires (const HvState& hv, const IntermediateHv& i_hv, const ParsHv& p_hv) {
    hv.adults;
    hv.prevalence;
    hv.mult_no_art;
    hv.mult_art;
    hv.new_inf_vrs;


    i_hv.r_mult;


    i_hv.totpop_deaths_background;

    i_hv.prep_effect;

    i_hv.pop_1549;

    i_hv.dp_totpop_deaths_background;
    i_hv.dp_totpop_1549;
    i_hv.dp_pop_1549;
    i_hv.dp_entrants_age_15;
    i_hv.dp_aging_50;
    i_hv.dp_aging_denom_1549;

    i_hv.background_death_rate;
    i_hv.migration_rate;
    i_hv.rate_aging_50;

    i_hv.b_riskgroup_proportions;
    i_hv.b_behave_change_rate;

    i_hv.hiv_lambda;
    i_hv.hiv_mu;
    i_hv.art_alpha;

    i_hv.b_riskgroup_proportions;

    i_hv.i_condom_prop;
    i_hv.i_num_partners;
    i_hv.i_age_first_sex;

    i_hv.adj_coverage;
    i_hv.adj_coverage_prod;

    i_hv. vac_effect;

  
    //i_hv.totpop_1549;
    //i_hv.pop_1549;
    //i_hv.entrants_age_15;
    //i_hv.aging_50;
    //i_hv.aging_denom_1549;

    //p_hv.epi_start_year;
    //p_hv.b_balance_sex_acts;
    //p_hv.epi_months_in_primary;
   // p_hv.epi_initial_pulse;
    p_hv.epi_redwhen_circum;
    p_hv.prep_cov;
    hv.cured_prop;

    p_hv.b_condom_prop;
    p_hv.b_behav_properties;
    p_hv.b_sex_acts;
    p_hv.b_num_partners;
    p_hv.b_incr_recruit;
    p_hv.b_married_prop;
    p_hv.b_age_first_sex;
    p_hv.b_idu_share_prop;
    p_hv.rn_poc_cov;
    p_hv.rn_vac_params;
    p_hv.rn_vac_coverage_all;

    //p_hv.rn_vac_cov_type;
    //p_hv.rn_vac_targetting;

    p_hv.epi_infectiousness;
    p_hv.epi_inf_mult_art;

    p_hv.prep_method_mix;
    p_hv.prep_effectiveness;
    p_hv.b_foi_idu;

    p_hv.rn_coverage;
    p_hv.hv_impact_matrix;

    p_hv.rn_unit_costs;



  }
inline HvDebugInfo capture_hv(const HvState& hv, const IntermediateHv& i_hv,
  const ParsHv& p_hv);

template <class HaState, class AnyIntermediate, class HaPars>
  requires requires (const HaState& ha, const HaPars& p_ha) {
    ha.p_hivpop;
    ha.h_hivpop;
    ha.hiv_births;
    p_ha.input_adult_incidence_rate;
    p_ha.local_adj_factor;
    p_ha.cd4_mortality;

  }
inline HaDebugInfo capture_ha(const HaState& ha, const AnyIntermediate& i_any,
  const HaPars& p_ha);

template <class HcState, class IntermediateHc, class ParsHc>
  requires requires (const HcState& hc, const IntermediateHc& i_hc, const ParsHc& p_hc) {
    hc.hc1_hivpop;
    i_hc.hc_posthivmort;
    //p_hc.hc_nosocomial;
  }
inline HcDebugInfo capture_hc(const HcState& hc, const IntermediateHc& i_hc,
  const ParsHc& p_hc);

template <class State, class Intermediate, class Pars>
  requires requires (const State& state, const Intermediate& intermediate, const Pars& pars) {
    state.dp; intermediate.dp; pars.dp;
    state.hv; intermediate.hv; pars.hv;
    state.ha; pars.ha;
    state.hc; intermediate.hc; pars.hc;
  }
inline ModelDebugInfo capture_model(const State& state,
  const Intermediate& intermediate, const Pars& pars);


// ---------------------------------------------------------------------------
// Global non-template debug helpers — callable from the VS Code Debug Console.
// Works with any nda array of any concrete type; no hardcoded variable names.
//
// Usage (paste into Debug Console while paused at a breakpoint):
//
//   nda_print_4d(arr.base(),
//       arr.shape().dim(0), arr.shape().dim(1),
//       arr.shape().dim(2), arr.shape().dim(3),
//       0, 0, 0, 0)       // non-negative = single index, -1 = all elements
//
//   nda_print_2d(arr.base(), arr.shape().dim(0), arr.shape().dim(1))
//
// Match the rank suffix (1d..6d) to the actual rank of the array.
// ---------------------------------------------------------------------------

#ifdef _MSC_VER
// dllexport forces the linker to retain the symbol even when never called from source.
// Without it, inline functions with no callers are silently stripped as dead code.
#  define NDA_DBG_NOINLINE __declspec(noinline) __declspec(dllexport)
#else
// 'used' prevents the compiler/linker from eliminating uncalled functions.
#  define NDA_DBG_NOINLINE __attribute__((noinline)) __attribute__((used))
#endif

// Declare OutputDebugStringA without pulling in all of <windows.h>.
#ifdef _WIN32
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);
#endif

NDA_DBG_NOINLINE inline void nda_print_impl(
    const double* base, int rank,
    const nda::index_t* mins,
    const nda::index_t* extents,
    const nda::index_t* strides,
    const int* sel) {
  int lo[8] = {}, hi[8] = {}, st[8] = {};
  for (int d = 0; d < rank; ++d) {
    int mn = static_cast<int>(mins[d]);
    int mx = mn + static_cast<int>(extents[d]);
    st[d]  = static_cast<int>(strides[d]);
    lo[d]  = (sel[d] < 0) ? mn : std::max(sel[d], mn);
    hi[d]  = (sel[d] < 0) ? mx : std::min(sel[d] + 1, mx);
    if (hi[d] < lo[d]) hi[d] = lo[d];
  }
  for (int d = rank; d < 8; ++d) { lo[d] = 0; hi[d] = 1; st[d] = 0; }
  for (int i0=lo[0];i0<hi[0];++i0)
  for (int i1=lo[1];i1<hi[1];++i1)
  for (int i2=lo[2];i2<hi[2];++i2)
  for (int i3=lo[3];i3<hi[3];++i3)
  for (int i4=lo[4];i4<hi[4];++i4)
  for (int i5=lo[5];i5<hi[5];++i5)
  for (int i6=lo[6];i6<hi[6];++i6)
  for (int i7=lo[7];i7<hi[7];++i7) {
    std::ptrdiff_t off = (std::ptrdiff_t)i0*st[0] + (std::ptrdiff_t)i1*st[1]
                       + (std::ptrdiff_t)i2*st[2] + (std::ptrdiff_t)i3*st[3]
                       + (std::ptrdiff_t)i4*st[4] + (std::ptrdiff_t)i5*st[5]
                       + (std::ptrdiff_t)i6*st[6] + (std::ptrdiff_t)i7*st[7];
    int idx[8] = {i0,i1,i2,i3,i4,i5,i6,i7};
    char _nda_buf[256];
    int _nda_pos = 0;
    _nda_pos += std::snprintf(_nda_buf + _nda_pos, sizeof(_nda_buf) - _nda_pos, "nda(");
    for (int d = 0; d < rank; ++d)
      _nda_pos += std::snprintf(_nda_buf + _nda_pos, sizeof(_nda_buf) - _nda_pos,
          "%d%s", idx[d], d+1<rank ? "," : "");
    std::snprintf(_nda_buf + _nda_pos, sizeof(_nda_buf) - _nda_pos, ") = %g\n", base[off]);
#ifdef _WIN32
    OutputDebugStringA(_nda_buf);
#else
    std::fputs(_nda_buf, stdout);
    std::fflush(stdout);
#endif
  }
}

// ---------------------------------------------------------------------------
// NdaInfo — captures an nda array into a plain non-template struct so that
// non-template functions (callable from the debug console) can print it.
// ---------------------------------------------------------------------------
struct NdaInfo {
  const double* base;
  int rank;
  nda::index_t mins[8];
  nda::index_t extents[8];
  nda::index_t strides[8];
};

template <class Array>
inline NdaInfo nda_capture(const Array& arr) {
  constexpr int R = (int)Array::shape_type::rank();
  NdaInfo info{};
  info.base = arr.base();
  info.rank = R;
  for (int d = 0; d < R; ++d) {
    const auto dim = arr.shape().dim(d);
    info.mins[d] = dim.min();
    info.extents[d] = dim.extent();
    info.strides[d] = dim.stride();
  }
  return info;
}

struct DpDebugInfo {
  // State
  NdaInfo p_totpop;
  NdaInfo p_deaths_background_totpop;
  double births;

  // Intermediate
  NdaInfo migration_rate;

  // Pars
  NdaInfo base_pop;
  NdaInfo survival_probability;
  NdaInfo net_migration;
  NdaInfo age_specific_fertility_rate;
  NdaInfo births_sex_prop;
  NdaInfo total_fertility_rate;
};


struct HvDebugInfo {
  // State
  NdaInfo adults;
  NdaInfo mult_no_art;
  NdaInfo mult_art;
  NdaInfo r_mult;
  NdaInfo prevalence;
  NdaInfo new_inf_vrs;


  // Intermediate
  NdaInfo dp_totpop_1549;
  NdaInfo migration_rate;
  NdaInfo dp_totpop_deaths_background;
  NdaInfo background_death_rate;
  NdaInfo deaths_art;
  NdaInfo dp_entrants_age_15;
  NdaInfo rate_aging_50;
  NdaInfo hiv_exit_rates;
  NdaInfo art_exit_rates;
  NdaInfo hiv_stage_exits;
  NdaInfo hiv_stage_progressors;
  NdaInfo art_stage_exits;
  NdaInfo stage_entrants;
  NdaInfo migrants;
  NdaInfo new_vaccinations;
  NdaInfo hiv_mu;
  NdaInfo hiv_lambda;
  NdaInfo art_alpha;
  NdaInfo dp_hiv_cd4_mort_no_art;
  NdaInfo dp_hiv_cd4_mort_art;
  NdaInfo dp_hiv_cd4_progression;
  NdaInfo aging_15;
  NdaInfo dp_aging_50;
  NdaInfo dp_pop_sex_age_hiv;
  NdaInfo dp_pop_1549;
  NdaInfo pop_1549;
  NdaInfo dp_pop_1549_hiv;
  NdaInfo dp_pop_1549_art;
  NdaInfo dp_aging_denom_1549;
  NdaInfo vac_params;
  NdaInfo vac_effect;
  NdaInfo art_coverage_rg;
  NdaInfo prep_effect;
  NdaInfo adj_coverage;

  NdaInfo b_riskgroup_proportions;
  NdaInfo b_behave_change_rate;

  NdaInfo i_condom_prop;
  NdaInfo i_num_partners;
  NdaInfo i_age_first_sex;

  NdaInfo epi_redwhen_circum;
  NdaInfo prep_cov;
  NdaInfo cured_prop;


  // Pars
  //int epi_start_year;
  //int b_balance_sex_acts;

  //double epi_months_in_primary;
  //double epi_initial_pulse;
  NdaInfo b_condom_prop;
  NdaInfo b_behav_properties;
  NdaInfo b_sex_acts;
  NdaInfo b_num_partners;
  NdaInfo b_incr_recruit;
  NdaInfo b_married_prop;
  NdaInfo b_age_first_sex;
  NdaInfo b_idu_share_prop;
  NdaInfo rn_poc_cov;
  NdaInfo rn_vac_params;
  NdaInfo rn_vac_coverage_all;
  //int rn_vac_cov_type;
  //int rn_vac_targetting;
  NdaInfo epi_infectiousness;
  NdaInfo epi_inf_mult_art;
  NdaInfo prep_method_mix;
  NdaInfo prep_effectiveness;
  NdaInfo b_foi_idu;
  NdaInfo rn_coverage; 
  NdaInfo hv_impact_matrix; 
  NdaInfo adj_coverage_prod;
  NdaInfo rn_unit_costs;


};

struct HaDebugInfo {
  // State
  NdaInfo p_hivpop;
  NdaInfo p_deaths_background_hivpop;
  NdaInfo h_hivpop;
  NdaInfo h_artpop;
  NdaInfo h_hiv_deaths_no_art;
  NdaInfo h_deaths_excess_nonaids_no_art;
  NdaInfo p_infections;
  NdaInfo h_hiv_deaths_art;
  NdaInfo h_deaths_excess_nonaids_on_art;
  NdaInfo h_art_initiation;
  NdaInfo p_hiv_deaths;
  NdaInfo p_deaths_excess_nonaids;
  NdaInfo p_net_migration_hivpop;
  NdaInfo hiv_births_by_mat_age;
  double hiv_births;
  NdaInfo prevalence_15to49_hts;
  NdaInfo incidence_15to49_hts;
  NdaInfo artcoverage_15to49_hts;

  // Pars (selected)
  int incidence_model_choice;
  NdaInfo input_adult_incidence_rate;
  NdaInfo transmission_rate_hts;
  double initial_incidence;
  int epidemic_start_hts;
  double relative_infectiousness_art;
  NdaInfo incidence_rate_ratio_age;
  NdaInfo incidence_rate_ratio_sex;
  NdaInfo cd4_mortality;
  NdaInfo cd4_progression;
  NdaInfo cd4_initial_distribution;
  NdaInfo art_mortality;
  NdaInfo art_mortality_time_rate_ratio;
  NdaInfo cd4_nonaids_excess_mort;
  NdaInfo art_nonaids_excess_mort;
  NdaInfo dropout_rate;
  NdaInfo adults_on_art;
  NdaInfo fert_mult_by_age;
  NdaInfo fert_mult_off_art;
  NdaInfo fert_mult_on_art;
  double local_adj_factor;
};

struct HcDebugInfo {
  // State
  NdaInfo hc1_hivpop;
  NdaInfo hc2_hivpop;
  NdaInfo hc1_artpop;
  NdaInfo hc2_artpop;
  NdaInfo hc1_noart_aids_deaths;
  NdaInfo hc2_noart_aids_deaths;
  NdaInfo hc1_art_aids_deaths;
  NdaInfo hc2_art_aids_deaths;
  NdaInfo hc_art_init;
  NdaInfo hc_art_need_init;
  double ctx_need;
  NdaInfo infection_by_type;
  NdaInfo mtct_by_source_tr;
  NdaInfo mtct_by_source_women;
  NdaInfo mtct_by_source_hc_infections;
  NdaInfo pmtct_coverage_at_delivery;

  // Intermediate
  NdaInfo age15_hivpop;
  NdaInfo age15_artpop;
  NdaInfo hc_posthivmort;
  NdaInfo hc_grad;
  NdaInfo eligible;
  NdaInfo unmet_need;
  NdaInfo total_need;
  NdaInfo on_art;
  NdaInfo total_art_previous;
  NdaInfo total_art_current;
  NdaInfo hc_art_grad;
  NdaInfo hc_art_scalar;
  NdaInfo hc_initByAge;
  NdaInfo hc_adj;
  NdaInfo hc_art_deaths;
  NdaInfo hc_hiv_dist;
  NdaInfo hc_hiv_total;
  NdaInfo art_ltfu_grad;
  NdaInfo p_hiv_neg_pop;
  NdaInfo PMTCT_coverage;
  NdaInfo PMTCT_not_retained;
  NdaInfo bf_transmission_rate;
  double retained;
  double hc_death_rate;
  NdaInfo ctx_mean;

  // Pars (selected)
  //NdaInfo hc_nosocomial;
  NdaInfo hc1_cd4_dist;
  NdaInfo hc1_cd4_mort;
  NdaInfo hc2_cd4_mort;
  NdaInfo hc1_cd4_prog;
  NdaInfo hc2_cd4_prog;
  NdaInfo ctx_val;
  NdaInfo hc_art_mort_rr;
  NdaInfo hc1_art_mort;
  NdaInfo hc2_art_mort;
  NdaInfo hc_art_val;
  NdaInfo hc_art_init_dist;
  NdaInfo PMTCT;
  NdaInfo vertical_transmission_rate;
  NdaInfo PMTCT_transmission_rate;
  NdaInfo PMTCT_dropout;
  NdaInfo breastfeeding_duration_art;
  NdaInfo breastfeeding_duration_no_art;
  NdaInfo infant_pop;
  NdaInfo mat_hiv_births;
  NdaInfo prop_lt200;
  NdaInfo prop_gte350;
  NdaInfo abortion;
  NdaInfo patients_reallocated;
  NdaInfo hc_art_ltfu;
  NdaInfo adult_female_infections;
  NdaInfo adult_female_hivnpop;
  NdaInfo total_births;
  NdaInfo ctx_effect;
  int hc_art_start;
  NdaInfo hc_age_specific_fertility_rate;
};

struct ModelDebugInfo {
  DpDebugInfo dp;
  HvDebugInfo hv;
  HaDebugInfo ha;
  HcDebugInfo hc;
};



template <class DpState, class IntermediateDp, class ParsDp>
  requires requires (const DpState& dp, const IntermediateDp& i_dp, const ParsDp& p_dp) {
    dp.p_totpop;
    i_dp.migration_rate;
    p_dp.total_fertility_rate;
  }
inline DpDebugInfo capture_dp(const DpState& dp, const IntermediateDp& i_dp,
    const ParsDp& p_dp) {
  DpDebugInfo out{};
  out.p_totpop = nda_capture(dp.p_totpop);
  out.p_deaths_background_totpop = nda_capture(dp.p_deaths_background_totpop);
  out.births = static_cast<double>(dp.births);

  out.migration_rate = nda_capture(i_dp.migration_rate);

  out.base_pop = nda_capture(p_dp.base_pop);
  out.survival_probability = nda_capture(p_dp.survival_probability);
  out.net_migration = nda_capture(p_dp.net_migration);
  out.age_specific_fertility_rate = nda_capture(p_dp.age_specific_fertility_rate);
  out.births_sex_prop = nda_capture(p_dp.births_sex_prop);
  out.total_fertility_rate = nda_capture(p_dp.total_fertility_rate);

  return out;
}

template <class HvState, class IntermediateHv, class ParsHv>
  requires requires (const HvState& hv, const IntermediateHv& i_hv, const ParsHv& p_hv) {

    hv.adults;
    hv.prevalence;

    hv.mult_no_art;
    hv.mult_art;
    hv.new_inf_vrs;

    i_hv.r_mult;

    i_hv.prep_effect;

    i_hv.pop_1549;

    i_hv.dp_totpop_deaths_background;
    i_hv.dp_totpop_1549;
    i_hv.dp_pop_1549;
    i_hv.dp_entrants_age_15;
    i_hv.dp_aging_50;
    i_hv.dp_aging_denom_1549;

    i_hv.background_death_rate;
    i_hv.migration_rate;
    i_hv.rate_aging_50;

    i_hv.b_riskgroup_proportions;
    i_hv.b_behave_change_rate;

    i_hv.hiv_lambda;
    i_hv.hiv_mu;
    i_hv.art_alpha;

    i_hv.i_condom_prop;
    i_hv.i_num_partners;
    i_hv.i_age_first_sex;

    p_hv.epi_redwhen_circum;
    p_hv.prep_cov;
    hv.cured_prop;

    i_hv.adj_coverage;
    i_hv.adj_coverage_prod;

    i_hv.vac_effect;




    //i_hv.totpop_1549;
    //i_hv.pop_1549;
    //i_hv.entrants_age_15;
    //i_hv.aging_50;
    //i_hv.aging_denom_1549;


/*     i_hv.totpop_deaths_background
    i_hv.totpop_1549
    i_hv.pop_1549
    i_hv.pop_1549_hiv
    i_hv.pop_1549_art
    i_hv.hiv_cd4_mort_no_art
    i_hv.hiv_cd4_mort_art
    i_hv.hiv_cd4_progression
    i_hv.pop_sex_age_hiv
    i_hv.entrants_age_15
    i_hv.aging_50
    i_hv.aging_denom_1549
    i_hv.migration_num
    i_hv.migration_denom */


    //p_hv.epi_start_year;
    //p_hv.b_balance_sex_acts;
   // p_hv.epi_months_in_primary;
    //p_hv.b_balance_sex_acts;
    //p_hv.epi_initial_pulse;
    p_hv.b_condom_prop;
    p_hv.b_behav_properties;
    p_hv.b_sex_acts;
    p_hv.b_num_partners;
    p_hv.b_incr_recruit;
    p_hv.b_married_prop;
    p_hv.b_age_first_sex;
    p_hv.b_idu_share_prop;
    p_hv.rn_poc_cov;
    p_hv.rn_vac_params;
    p_hv.rn_vac_coverage_all;
   // p_hv.rn_vac_cov_type;
    //p_hv.rn_vac_targetting;
    p_hv.epi_infectiousness;
    p_hv.epi_inf_mult_art;
    p_hv.prep_method_mix;
    p_hv.prep_effectiveness;
    p_hv.b_foi_idu;
    p_hv.rn_coverage;
    p_hv.hv_impact_matrix;
    p_hv.rn_unit_costs;
  

  }
inline HvDebugInfo capture_hv(const HvState& hv, const IntermediateHv& i_hv,
    const ParsHv& p_hv) {
  HvDebugInfo out{};

  out.adults = nda_capture(hv.adults);
  out.prevalence = nda_capture(hv.prevalence);

  out.mult_no_art = nda_capture(hv.mult_no_art);
  out.mult_art = nda_capture(hv.mult_art);
 
  out.r_mult = nda_capture(i_hv.r_mult);

  out.new_inf_vrs = nda_capture(hv.new_inf_vrs);

  out.prep_effect = nda_capture(i_hv.prep_effect);

  out.pop_1549 = nda_capture(i_hv.pop_1549);

  out.dp_totpop_deaths_background = nda_capture(i_hv.dp_totpop_deaths_background);
  out.dp_totpop_1549 = nda_capture(i_hv.dp_totpop_1549);
  out.dp_pop_1549 = nda_capture(i_hv.dp_pop_1549);
  out.dp_entrants_age_15 = nda_capture(i_hv.dp_entrants_age_15);
  out.dp_aging_50 = nda_capture(i_hv.dp_aging_50);
  out.dp_aging_denom_1549 = nda_capture(i_hv.dp_aging_denom_1549);

  out.background_death_rate = nda_capture(i_hv.background_death_rate);
  out.migration_rate = nda_capture(i_hv.migration_rate);
  out.rate_aging_50 = nda_capture(i_hv.rate_aging_50);

  out.b_riskgroup_proportions = nda_capture(i_hv.b_riskgroup_proportions);
  out.b_behave_change_rate = nda_capture(i_hv.b_behave_change_rate);

  out.hiv_mu = nda_capture(i_hv.hiv_mu);
  out.hiv_lambda = nda_capture(i_hv.hiv_lambda);
  out.art_alpha = nda_capture(i_hv.art_alpha);

  out.i_condom_prop = nda_capture(i_hv.i_condom_prop);
  out.i_num_partners = nda_capture(i_hv.i_num_partners);
  out.i_age_first_sex = nda_capture(i_hv.i_age_first_sex);

  out.epi_redwhen_circum = nda_capture(p_hv.epi_redwhen_circum);
  out.prep_cov = nda_capture(p_hv.prep_cov);
  out.cured_prop = nda_capture(hv.cured_prop);

  out.adj_coverage = nda_capture(i_hv.adj_coverage);
  out.hv_impact_matrix = nda_capture(p_hv.hv_impact_matrix);
  out.adj_coverage_prod = nda_capture(i_hv.adj_coverage_prod);
  out.rn_unit_costs = nda_capture(p_hv.rn_unit_costs);

   out.vac_effect = nda_capture(i_hv.vac_effect);




  //out.riskgroup_proportions = nda_capture(i_hv.entrants_age_15);
  //out.riskgroup_proportions = nda_capture(i_hv.aging_50);
  //out.riskgroup_proportions = nda_capture(i_hv.aging_denom_1549);






  /*
  out.totpop_1549 = nda_capture(i_hv.totpop_1549);
  out.migration_num = nda_capture(i_hv.migration_num);
  out.migration_denom = nda_capture(i_hv.migration_denom);
  out.migration_rate = nda_capture(i_hv.migration_rate);
  out.totpop_deaths_background = nda_capture(i_hv.totpop_deaths_background);
  out.background_death_rate = nda_capture(i_hv.background_death_rate);
  out.deaths_art = nda_capture(i_hv.deaths_art);
  out.entrants_age_15 = nda_capture(i_hv.entrants_age_15);
  out.rate_aging_50 = nda_capture(i_hv.rate_aging_50);
  out.hiv_exit_rates = nda_capture(i_hv.hiv_exit_rates);
  out.art_exit_rates = nda_capture(i_hv.art_exit_rates);
  out.hiv_stage_exits = nda_capture(i_hv.hiv_stage_exits);
  out.hiv_stage_progressors = nda_capture(i_hv.hiv_stage_progressors);
  out.art_stage_exits = nda_capture(i_hv.art_stage_exits);
  out.stage_entrants = nda_capture(i_hv.stage_entrants);
  out.migrants = nda_capture(i_hv.migrants);
  out.new_vaccinations = nda_capture(i_hv.new_vaccinations);
  out.hiv_mu = nda_capture(i_hv.hiv_mu);
  out.hiv_lambda = nda_capture(i_hv.hiv_lambda);
  out.art_alpha = nda_capture(i_hv.art_alpha);
  out.hiv_cd4_mort_no_art = nda_capture(i_hv.hiv_cd4_mort_no_art);
  out.hiv_cd4_mort_art = nda_capture(i_hv.hiv_cd4_mort_art);
  out.hiv_cd4_progression = nda_capture(i_hv.hiv_cd4_progression);
  out.aging_15 = nda_capture(i_hv.aging_15);
  out.aging_50 = nda_capture(i_hv.aging_50);
  out.pop_sex_age_hiv = nda_capture(i_hv.pop_sex_age_hiv);
  out.pop_1549 = nda_capture(i_hv.pop_1549);
  out.pop_1549_hiv = nda_capture(i_hv.pop_1549_hiv);
  out.pop_1549_art = nda_capture(i_hv.pop_1549_art);
  out.aging_denom_1549 = nda_capture(i_hv.aging_denom_1549);
  out.vac_params = nda_capture(i_hv.vac_params);
  out.vac_effect = nda_capture(i_hv.vac_effect);
  out.art_coverage_rg = nda_capture(i_hv.art_coverage_rg); */

  //out.epi_start_year = static_cast<int>(p_hv.epi_start_year);
  //out.epi_months_in_primary = static_cast<double>(p_hv.epi_months_in_primary);
  //out.b_balance_sex_acts = static_cast<int>(p_hv.b_balance_sex_acts);
  //out.epi_initial_pulse = static_cast<double>(p_hv.epi_initial_pulse);
  out.b_condom_prop = nda_capture(p_hv.b_condom_prop);
  out.b_behav_properties = nda_capture(p_hv.b_behav_properties);
  out.b_sex_acts = nda_capture(p_hv.b_sex_acts);
  out.b_num_partners = nda_capture(p_hv.b_num_partners);
  out.b_incr_recruit = nda_capture(p_hv.b_incr_recruit);
  out.b_married_prop = nda_capture(p_hv.b_married_prop);
  out.b_age_first_sex = nda_capture(p_hv.b_age_first_sex);
  out.b_idu_share_prop = nda_capture(p_hv.b_idu_share_prop);
  out.rn_poc_cov = nda_capture(p_hv.rn_poc_cov);
  out.rn_vac_params = nda_capture(p_hv.rn_vac_params);
  out.rn_vac_coverage_all = nda_capture(p_hv.rn_vac_coverage_all);
  //out.rn_vac_cov_type = static_cast<int>(p_hv.rn_vac_cov_type);
  //out.rn_vac_targetting = static_cast<int>(p_hv.rn_vac_targetting);
  out.epi_infectiousness =nda_capture(p_hv.epi_infectiousness);
  out.epi_inf_mult_art = nda_capture(p_hv.epi_inf_mult_art);

  out.prep_method_mix = nda_capture(p_hv.prep_method_mix);
  out.prep_effectiveness = nda_capture(p_hv.prep_effectiveness);
  out.b_foi_idu = nda_capture(p_hv.b_foi_idu);

  out.rn_coverage = nda_capture(p_hv.rn_coverage);

  return out;
}

template <class HaState, class AnyIntermediate, class HaPars>
  requires requires (const HaState& ha, const HaPars& p_ha) {
    ha.p_hivpop;
    ha.h_hivpop;
    ha.hiv_births;
    p_ha.input_adult_incidence_rate;
    p_ha.local_adj_factor;
    p_ha.cd4_mortality;
  }
inline HaDebugInfo capture_ha(const HaState& ha, const AnyIntermediate& i_any,
    const HaPars& p_ha) {
  (void)i_any;
  HaDebugInfo out{};

  out.p_hivpop = nda_capture(ha.p_hivpop);
  out.p_deaths_background_hivpop = nda_capture(ha.p_deaths_background_hivpop);
  out.h_hivpop = nda_capture(ha.h_hivpop);
  out.h_artpop = nda_capture(ha.h_artpop);
  out.h_hiv_deaths_no_art = nda_capture(ha.h_hiv_deaths_no_art);
  out.h_deaths_excess_nonaids_no_art = nda_capture(ha.h_deaths_excess_nonaids_no_art);
  out.p_infections = nda_capture(ha.p_infections);
  out.h_hiv_deaths_art = nda_capture(ha.h_hiv_deaths_art);
  out.h_deaths_excess_nonaids_on_art = nda_capture(ha.h_deaths_excess_nonaids_on_art);
  out.h_art_initiation = nda_capture(ha.h_art_initiation);
  out.p_hiv_deaths = nda_capture(ha.p_hiv_deaths);
  out.p_deaths_excess_nonaids = nda_capture(ha.p_deaths_excess_nonaids);
  out.p_net_migration_hivpop = nda_capture(ha.p_net_migration_hivpop);
  out.hiv_births_by_mat_age = nda_capture(ha.hiv_births_by_mat_age);
  out.hiv_births = static_cast<double>(ha.hiv_births);
  out.prevalence_15to49_hts = nda_capture(ha.prevalence_15to49_hts);
  out.incidence_15to49_hts = nda_capture(ha.incidence_15to49_hts);
  out.artcoverage_15to49_hts = nda_capture(ha.artcoverage_15to49_hts);

  out.incidence_model_choice = static_cast<int>(p_ha.incidence_model_choice);
  out.input_adult_incidence_rate = nda_capture(p_ha.input_adult_incidence_rate);
  out.transmission_rate_hts = nda_capture(p_ha.transmission_rate_hts);
  out.initial_incidence = static_cast<double>(p_ha.initial_incidence);
  out.epidemic_start_hts = static_cast<int>(p_ha.epidemic_start_hts);
  out.relative_infectiousness_art = static_cast<double>(p_ha.relative_infectiousness_art);
  out.incidence_rate_ratio_age = nda_capture(p_ha.incidence_rate_ratio_age);
  out.incidence_rate_ratio_sex = nda_capture(p_ha.incidence_rate_ratio_sex);
  out.cd4_mortality = nda_capture(p_ha.cd4_mortality);
  out.cd4_progression = nda_capture(p_ha.cd4_progression);
  out.cd4_initial_distribution = nda_capture(p_ha.cd4_initial_distribution);
  out.art_mortality = nda_capture(p_ha.art_mortality);
  out.art_mortality_time_rate_ratio = nda_capture(p_ha.art_mortality_time_rate_ratio);
  out.cd4_nonaids_excess_mort = nda_capture(p_ha.cd4_nonaids_excess_mort);
  out.art_nonaids_excess_mort = nda_capture(p_ha.art_nonaids_excess_mort);
  out.dropout_rate = nda_capture(p_ha.dropout_rate);
  out.adults_on_art = nda_capture(p_ha.adults_on_art);
  out.fert_mult_by_age = nda_capture(p_ha.fert_mult_by_age);
  out.fert_mult_off_art = nda_capture(p_ha.fert_mult_off_art);
  out.fert_mult_on_art = nda_capture(p_ha.fert_mult_on_art);
  out.local_adj_factor = static_cast<double>(p_ha.local_adj_factor);
  

  return out;
}

template <class HcState, class IntermediateHc, class ParsHc>
  requires requires (const HcState& hc, const IntermediateHc& i_hc, const ParsHc& p_hc) {
    hc.hc1_hivpop;
    i_hc.hc_posthivmort;
    //p_hc.hc_nosocomial;
  }
inline HcDebugInfo capture_hc(const HcState& hc, const IntermediateHc& i_hc,
    const ParsHc& p_hc) {
  HcDebugInfo out{};

  out.hc1_hivpop = nda_capture(hc.hc1_hivpop);
  out.hc2_hivpop = nda_capture(hc.hc2_hivpop);
  out.hc1_artpop = nda_capture(hc.hc1_artpop);
  out.hc2_artpop = nda_capture(hc.hc2_artpop);
  out.hc1_noart_aids_deaths = nda_capture(hc.hc1_noart_aids_deaths);
  out.hc2_noart_aids_deaths = nda_capture(hc.hc2_noart_aids_deaths);
  out.hc1_art_aids_deaths = nda_capture(hc.hc1_art_aids_deaths);
  out.hc2_art_aids_deaths = nda_capture(hc.hc2_art_aids_deaths);
  out.hc_art_init = nda_capture(hc.hc_art_init);
  out.hc_art_need_init = nda_capture(hc.hc_art_need_init);
  out.ctx_need = static_cast<double>(hc.ctx_need);
  out.infection_by_type = nda_capture(hc.infection_by_type);
  out.mtct_by_source_tr = nda_capture(hc.mtct_by_source_tr);
  out.mtct_by_source_women = nda_capture(hc.mtct_by_source_women);
  out.mtct_by_source_hc_infections = nda_capture(hc.mtct_by_source_hc_infections);
  out.pmtct_coverage_at_delivery = nda_capture(hc.pmtct_coverage_at_delivery);

  out.age15_hivpop = nda_capture(i_hc.age15_hivpop);
  out.age15_artpop = nda_capture(i_hc.age15_artpop);
  out.hc_posthivmort = nda_capture(i_hc.hc_posthivmort);
  out.hc_grad = nda_capture(i_hc.hc_grad);
  out.eligible = nda_capture(i_hc.eligible);
  out.unmet_need = nda_capture(i_hc.unmet_need);
  out.total_need = nda_capture(i_hc.total_need);
  out.on_art = nda_capture(i_hc.on_art);
  out.total_art_previous = nda_capture(i_hc.total_art_previous);
  out.total_art_current = nda_capture(i_hc.total_art_current);
  out.hc_art_grad = nda_capture(i_hc.hc_art_grad);
  out.hc_art_scalar = nda_capture(i_hc.hc_art_scalar);
  out.hc_initByAge = nda_capture(i_hc.hc_initByAge);
  out.hc_adj = nda_capture(i_hc.hc_adj);
  out.hc_art_deaths = nda_capture(i_hc.hc_art_deaths);
  out.hc_hiv_dist = nda_capture(i_hc.hc_hiv_dist);
  out.hc_hiv_total = nda_capture(i_hc.hc_hiv_total);
  out.art_ltfu_grad = nda_capture(i_hc.art_ltfu_grad);
  out.p_hiv_neg_pop = nda_capture(i_hc.p_hiv_neg_pop);
  out.PMTCT_coverage = nda_capture(i_hc.PMTCT_coverage);
  out.PMTCT_not_retained = nda_capture(i_hc.PMTCT_not_retained);
  out.bf_transmission_rate = nda_capture(i_hc.bf_transmission_rate);
  out.retained = static_cast<double>(i_hc.retained);
  out.hc_death_rate = static_cast<double>(i_hc.hc_death_rate);
  out.ctx_mean = nda_capture(i_hc.ctx_mean);

  //out.hc_nosocomial = nda_capture(p_hc.hc_nosocomial);
  out.hc1_cd4_dist = nda_capture(p_hc.hc1_cd4_dist);
  out.hc1_cd4_mort = nda_capture(p_hc.hc1_cd4_mort);
  out.hc2_cd4_mort = nda_capture(p_hc.hc2_cd4_mort);
  out.hc1_cd4_prog = nda_capture(p_hc.hc1_cd4_prog);
  out.hc2_cd4_prog = nda_capture(p_hc.hc2_cd4_prog);
  out.ctx_val = nda_capture(p_hc.ctx_val);
  out.hc_art_mort_rr = nda_capture(p_hc.hc_art_mort_rr);
  out.hc1_art_mort = nda_capture(p_hc.hc1_art_mort);
  out.hc2_art_mort = nda_capture(p_hc.hc2_art_mort);
  out.hc_art_val = nda_capture(p_hc.hc_art_val);
  out.hc_art_init_dist = nda_capture(p_hc.hc_art_init_dist);
  out.PMTCT = nda_capture(p_hc.PMTCT);
  out.vertical_transmission_rate = nda_capture(p_hc.vertical_transmission_rate);
  out.PMTCT_transmission_rate = nda_capture(p_hc.PMTCT_transmission_rate);
  out.PMTCT_dropout = nda_capture(p_hc.PMTCT_dropout);
  out.breastfeeding_duration_art = nda_capture(p_hc.breastfeeding_duration_art);
  out.breastfeeding_duration_no_art = nda_capture(p_hc.breastfeeding_duration_no_art);
  out.infant_pop = nda_capture(p_hc.infant_pop);
  out.mat_hiv_births = nda_capture(p_hc.mat_hiv_births);
  out.prop_lt200 = nda_capture(p_hc.prop_lt200);
  out.prop_gte350 = nda_capture(p_hc.prop_gte350);
  out.abortion = nda_capture(p_hc.abortion);
  out.patients_reallocated = nda_capture(p_hc.patients_reallocated);
  out.hc_art_ltfu = nda_capture(p_hc.hc_art_ltfu);
  out.adult_female_infections = nda_capture(p_hc.adult_female_infections);
  out.adult_female_hivnpop = nda_capture(p_hc.adult_female_hivnpop);
  out.total_births = nda_capture(p_hc.total_births);
  out.ctx_effect = nda_capture(p_hc.ctx_effect);
  out.hc_art_start = static_cast<int>(p_hc.hc_art_start);
  out.hc_age_specific_fertility_rate = nda_capture(p_hc.hc_age_specific_fertility_rate);

  return out;
}

template <class State, class Intermediate, class Pars>
  requires requires (const State& state, const Intermediate& intermediate, const Pars& pars) {
    state.dp; intermediate.dp; pars.dp;
    state.hv; intermediate.hv; pars.hv;
    state.ha; pars.ha;
    state.hc; intermediate.hc; pars.hc;
  }
inline ModelDebugInfo capture_model(const State& state,
    const Intermediate& intermediate, const Pars& pars) {
  ModelDebugInfo out{};
  out.dp = capture_dp(state.dp, intermediate.dp, pars.dp);
  out.hv = capture_hv(state.hv, intermediate.hv, pars.hv);
  // if constexpr (requires {
  //   capture_hv(state.hv, intermediate.hv, pars.hv);
  // }) {
  // }
  out.ha = capture_ha(state.ha, intermediate.hv, pars.ha);
  out.hc = capture_hc(state.hc, intermediate.hc, pars.hc);
  return out;
}

NDA_DBG_NOINLINE inline void nda_print_info_impl_bounds(
    const NdaInfo& info,
    const int* sel_lo,
    const int* sel_hi) {
  int lo[8] = {}, hi[8] = {}, st[8] = {};
  for (int d = 0; d < info.rank; ++d) {
    const int mn = static_cast<int>(info.mins[d]);
    const int mx = mn + static_cast<int>(info.extents[d]);
    st[d] = static_cast<int>(info.strides[d]);
    const bool all = (sel_lo[d] < 0) || (sel_hi[d] < 0);
    lo[d] = all ? mn : std::max(sel_lo[d], mn);
    hi[d] = all ? mx : std::min(sel_hi[d] + 1, mx);
    if (hi[d] < lo[d]) hi[d] = lo[d];
  }
  for (int d = info.rank; d < 8; ++d) { lo[d] = 0; hi[d] = 1; st[d] = 0; }
  for (int i0=lo[0];i0<hi[0];++i0)
  for (int i1=lo[1];i1<hi[1];++i1)
  for (int i2=lo[2];i2<hi[2];++i2)
  for (int i3=lo[3];i3<hi[3];++i3)
  for (int i4=lo[4];i4<hi[4];++i4)
  for (int i5=lo[5];i5<hi[5];++i5)
  for (int i6=lo[6];i6<hi[6];++i6)
  for (int i7=lo[7];i7<hi[7];++i7) {
    std::ptrdiff_t off = (std::ptrdiff_t)i0*st[0] + (std::ptrdiff_t)i1*st[1]
                       + (std::ptrdiff_t)i2*st[2] + (std::ptrdiff_t)i3*st[3]
                       + (std::ptrdiff_t)i4*st[4] + (std::ptrdiff_t)i5*st[5]
                       + (std::ptrdiff_t)i6*st[6] + (std::ptrdiff_t)i7*st[7];
    int idx[8] = {i0,i1,i2,i3,i4,i5,i6,i7};
    char _nda_buf[256];
    int _nda_pos = 0;
    _nda_pos += std::snprintf(_nda_buf + _nda_pos, sizeof(_nda_buf) - _nda_pos, "nda(");
    for (int d = 0; d < info.rank; ++d)
      _nda_pos += std::snprintf(_nda_buf + _nda_pos, sizeof(_nda_buf) - _nda_pos,
          "%d%s", idx[d], d+1<info.rank ? "," : "");
    std::snprintf(_nda_buf + _nda_pos, sizeof(_nda_buf) - _nda_pos, ") = %g\n", info.base[off]);
#ifdef _WIN32
    OutputDebugStringA(_nda_buf);
#else
    std::fputs(_nda_buf, stdout);
    std::fflush(stdout);
#endif
  }
}

// nda_print_info overloads — inclusive lo/hi bounds per dimension. Use -1 for "all".
NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info) {
  int sel_lo[] = {-1, -1, -1, -1, -1, -1, -1, -1};
  int sel_hi[] = {-1, -1, -1, -1, -1, -1, -1, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0) {
  int sel_lo[] = {lo0, -1, -1, -1, -1, -1, -1, -1};
  int sel_hi[] = {hi0, -1, -1, -1, -1, -1, -1, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0, int lo1, int hi1) {
  int sel_lo[] = {lo0, lo1, -1, -1, -1, -1, -1, -1};
  int sel_hi[] = {hi0, hi1, -1, -1, -1, -1, -1, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0, int lo1, int hi1, int lo2, int hi2) {
  int sel_lo[] = {lo0, lo1, lo2, -1, -1, -1, -1, -1};
  int sel_hi[] = {hi0, hi1, hi2, -1, -1, -1, -1, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0, int lo1, int hi1, int lo2, int hi2, int lo3, int hi3) {
  int sel_lo[] = {lo0, lo1, lo2, lo3, -1, -1, -1, -1};
  int sel_hi[] = {hi0, hi1, hi2, hi3, -1, -1, -1, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0, int lo1, int hi1, int lo2, int hi2, int lo3, int hi3, int lo4, int hi4) {
  int sel_lo[] = {lo0, lo1, lo2, lo3, lo4, -1, -1, -1};
  int sel_hi[] = {hi0, hi1, hi2, hi3, hi4, -1, -1, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0, int lo1, int hi1, int lo2, int hi2, int lo3, int hi3, int lo4, int hi4, int lo5, int hi5) {
  int sel_lo[] = {lo0, lo1, lo2, lo3, lo4, lo5, -1, -1};
  int sel_hi[] = {hi0, hi1, hi2, hi3, hi4, hi5, -1, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0, int lo1, int hi1, int lo2, int hi2, int lo3, int hi3, int lo4, int hi4, int lo5, int hi5, int lo6, int hi6) {
  int sel_lo[] = {lo0, lo1, lo2, lo3, lo4, lo5, lo6, -1};
  int sel_hi[] = {hi0, hi1, hi2, hi3, hi4, hi5, hi6, -1};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}

NDA_DBG_NOINLINE inline void nda_print_info(const NdaInfo& info,
    int lo0, int hi0, int lo1, int hi1, int lo2, int hi2, int lo3, int hi3, int lo4, int hi4, int lo5, int hi5, int lo6, int hi6, int lo7, int hi7) {
  int sel_lo[] = {lo0, lo1, lo2, lo3, lo4, lo5, lo6, lo7};
  int sel_hi[] = {hi0, hi1, hi2, hi3, hi4, hi5, hi6, hi7};
  nda_print_info_impl_bounds(info, sel_lo, sel_hi);
}
#undef NDA_DBG_NOINLINE
