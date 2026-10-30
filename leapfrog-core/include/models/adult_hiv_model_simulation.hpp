#pragma once

#include "../options.hpp"
#include "../generated/config_mixer.hpp"

namespace leapfrog {
namespace internal {

template<typename Config>
concept AdultHivModelSimulationEnabled = RunDemographicProjection<Config> && RunHivSimulation<Config>;

template<typename Config>
struct AdultHivModelSimulation {
  AdultHivModelSimulation(...) {};
};

template<AdultHivModelSimulationEnabled Config>
struct AdultHivModelSimulation<Config> {
  using real_type = typename Config::real_type;
  using ModelVariant = typename Config::ModelVariant;
  using SS = Config::SS;
  using Pars = Config::Pars;
  using State = Config::State;
  using Intermediate = Config::Intermediate;
  using Args = Config::Args;

  // private members of this struct
  private:
  // state space
  static constexpr int NS = SS::NS;
  static constexpr int pAG = SS::pAG;
  static constexpr int hDS = SS::hDS;
  static constexpr int hTS = SS::hTS;
  static constexpr int hAG = SS::hAG;
  static constexpr auto hAG_span = SS::hAG_span;
  static constexpr int PROJPERIOD_MIDYEAR = SS::PROJPERIOD_MIDYEAR;
  static constexpr int MALE = SS::MALE;
  static constexpr int FEMALE = SS::FEMALE;
  static constexpr int ART0MOS = SS::ART0MOS;
  static constexpr int p_idx_hiv_first_adult = SS::p_idx_hiv_first_adult;
  static constexpr int hIDX_15PLUS = SS::hIDX_15PLUS;
  static constexpr int hAG_fertility = SS::hAG_fertility;
  static constexpr int p_idx_fertility_first = SS::p_idx_fertility_first;
  static constexpr int p_fertility_age_groups = SS::p_fertility_age_groups;


  // function args
  int t;
  const Pars& pars;
  const State& state_curr;
  State& state_next;
  Intermediate& intermediate;
  const Options<real_type>& opts;

  // only exposing the constructor and some methods
  public:
  AdultHivModelSimulation(Args& args):
    t(args.t),
    pars(args.pars),
    state_curr(args.state_curr),
    state_next(args.state_next),
    intermediate(args.intermediate),
    opts(args.opts)
  {};

  void run_hiv_adult_pre_hiv_loop() {
    const auto& p_ha = pars.ha;
    auto& i_ha = intermediate.ha;

    // Lowest CD4 stage anyone is eligible at this year: every stage once special
    // populations or pregnant women are eligible regardless of CD4, stage 2
    // (WHO stage 3/4) when that is set, otherwise the CD4 threshold. The stage
    // ever eligible ratchets down over time. As EPP's age model
    // (AgeModel.run_hiv_model, anyelig_idx / everARTelig_idx) and eppasm.
    auto anyelig_at = [&](int tt) {
      if (p_ha.specpop_percelig(tt) > 0.0 || p_ha.pw_artelig(tt) > 0) return 0;
      if (p_ha.who34percelig > 0.0) return 2;
      return p_ha.idx_hm_elig(tt);
    };
    i_ha.anyelig_idx = anyelig_at(t);
    int ever = hDS;
    for (int tt = 1; tt <= t; ++tt) ever = std::min(ever, anyelig_at(tt));
    i_ha.everARTelig_idx = ever;

    // Births by fertile HIV age group for pregnant women's ART eligibility,
    // fixed for the year: the female population averaged over the start and
    // end of the year's demographic projection, before this year's HIV deaths,
    // times age-specific fertility (EPP's AgeModel.run_hiv_model births_by_ha;
    // eppasm). Computed here because p_totpop loses HIV deaths every time step.
    if (p_ha.pw_artelig(t) > 0) {
      const auto& p_dp = pars.dp;
      const auto& c_dp = state_curr.dp;
      const auto& n_dp = state_next.dp;
      int a = p_idx_hiv_first_adult;
      for (int hf = 0; hf < hAG_fertility; ++hf) {
        i_ha.pw_births_by_fertility_age(hf) = 0.0;
        for (int i = 0; i < hAG_span[hIDX_15PLUS + hf]; ++i, ++a) {
          i_ha.pw_births_by_fertility_age(hf) += 0.5 * (c_dp.p_totpop(a, FEMALE) + n_dp.p_totpop(a, FEMALE)) *
            p_dp.age_specific_fertility_rate(a - p_idx_fertility_first, t);
        }
      }
    }

    if (p_ha.incidence_model_choice == SS::INCIDMOD_DIRECTINCID_HTS) {

      // Note: In Spectrum, incidence rate by sex is calculated once per year, using
      // the previous year HIV negative population.
      // Incidence rate by age is calculated per time-step using the **current year**
      // HIV negative population, rather than the previous year HIV population.
      // Rob Glaubius, 5 August 2022: https://github.com/mrc-ide/leaptfrog/issues/18
      calculate_annual_incidence_rate_by_sex();
    }
  };

  void run_hiv_adult_hiv_loop(int hiv_step) {
    const auto& p_ha = pars.ha;
    auto& i_ha = intermediate.ha;

    nda::fill(i_ha.grad, 0.0);
    nda::fill(i_ha.grad_infections, 0.0);
    nda::fill(i_ha.gradART, 0.0);
    nda::fill(i_ha.h_hiv_deaths_age_sex, 0.0);
    nda::fill(i_ha.h_deaths_excess_nonaids_agesex, 0.0);
    run_disease_progression_and_mortality();

    if constexpr (ModelVariant::run_goals) {
      if (hiv_step==0) {
        state_next.hv.new_infections_dp = 0.0;//init new infections from DP
      }
      calc_new_infections_agesex_goals();

    } else {

      if (p_ha.incidence_model_choice == SS::INCIDMOD_DIRECTINCID_HTS) {
        calc_new_infections_agesex();
      } else if (p_ha.incidence_model_choice == SS::INCIDMOD_TRANSMRATE_HTS){
        calc_new_infections_incidmod_transmrate(hiv_step);
      } else {
        throw std::invalid_argument("Incidence model choice not vaild\n");
      }
    }

    add_new_hiv_infections();

    if (t >= opts.ts_art_start) {
      run_art_progression_and_mortality();
      run_h_art_initiation(hiv_step);

      if constexpr (ModelVariant::run_virgin) {
        // Note: must be run before run_update_art_adult() because it
        // relies on the proportion of population virgin before updating
        run_update_art_virgin();
      }
      run_update_art_adult();
    }

    if constexpr (ModelVariant::run_virgin) {
      // Note: must be run before run_update_hiv_adult() because it
      // relies on the proportion of population virgin before updating
      run_update_hiv_virgin();
    }
    run_update_hiv_adult();

    run_calc_p_hiv_deaths();
    if constexpr (ModelVariant::run_virgin) {
      run_remove_p_virgin_hiv_deaths();
    }
    run_remove_p_hiv_deaths();
    run_wlhiv_births();

    if constexpr (ModelVariant::run_goals) {
      if ((t > pars.hv.goals_base_year_idx) && (hiv_step == 0)) {
        apply_goals_cure_adults();
      }
    }

  };

  // private methods that we don't want people to call
  private:
  void calculate_annual_incidence_rate_by_sex() {

    const auto& p_ha = pars.ha;
    const auto& c_dp = state_curr.dp;
    const auto& c_ha = state_curr.ha;
    auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; ++s) {
      for (int a = p_ha.pIDX_INCIDPOP; a < p_ha.pIDX_INCIDPOP + p_ha.pAG_INCIDPOP; ++a) {
        i_ha.hiv_neg_aggregate(s) += c_dp.p_totpop(a, s) - c_ha.p_hivpop(a, s);
      }

      if constexpr(ModelVariant::run_virgin) {
        // Remove virgin HIV negative from at risk population
        const auto& c_vg = state_curr.vg;

        const int vg_min_incid_age = std::max(0, p_ha.pIDX_INCIDPOP - SS::p_idx_virginpop_first);
        const int vg_max_incid_age = std::min(SS::vAG, p_ha.pIDX_INCIDPOP + p_ha.pAG_INCIDPOP - SS::p_idx_virginpop_first);
        for (int va = vg_min_incid_age; va < vg_max_incid_age; ++va) {
          i_ha.hiv_neg_aggregate(s) -= c_vg.p_totpop_virgin(va, s) - c_vg.p_hivpop_virgin(va, s);
        }
      }
    }

    real_type incrr_wgt_denominator = i_ha.hiv_neg_aggregate(MALE) +
                            p_ha.incidence_rate_ratio_sex(t) * i_ha.hiv_neg_aggregate(FEMALE);
    real_type total_neg = i_ha.hiv_neg_aggregate(MALE) + i_ha.hiv_neg_aggregate(FEMALE);
    i_ha.incidence_rate_sex(MALE) = p_ha.input_adult_incidence_rate(t) * total_neg / incrr_wgt_denominator;
    i_ha.incidence_rate_sex(FEMALE) = i_ha.incidence_rate_sex(MALE) * p_ha.incidence_rate_ratio_sex(t);

  };

  void run_disease_progression_and_mortality() {
    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; ++s) {
      for (int ha = 0; ha < hAG; ++ha) {
        for (int hm = 0; hm < hDS; ++hm) {
          real_type cd4mx_scale = 1.0;
          if (p_ha.scale_cd4_mortality && t >= opts.ts_art_start &&
              hm >= i_ha.everARTelig_idx && n_ha.h_hivpop(hm, ha, s) > 0.0) {
            real_type artpop_hahm = 0.0;
            for (int hu = 0; hu < hTS; ++hu) {
              artpop_hahm += n_ha.h_artpop(hu, hm, ha, s);
            }
            cd4mx_scale = n_ha.h_hivpop(hm, ha, s) /
                          (n_ha.h_hivpop(hm, ha, s) + artpop_hahm);
          }

          auto deaths_hiv = cd4mx_scale * p_ha.cd4_mortality(hm, ha, s) * n_ha.h_hivpop(hm, ha, s);
          //capture the impact of AHD treament on hiv mortality
          if constexpr (ModelVariant::run_goals) {
            if ( (t > pars.hv.goals_base_year_idx) && (hm>=4) ) { // index 4 is CD4_100_199
              deaths_hiv *= intermediate.hv.AHD_Tx_Impact;
            }
          }

          i_ha.h_hiv_deaths_age_sex(ha, s) += opts.dt * deaths_hiv;
          n_ha.h_hiv_deaths_no_art(hm, ha, s) += opts.dt * deaths_hiv;

          auto deaths_excess_nonaids = p_ha.cd4_nonaids_excess_mort(hm, ha, s) * n_ha.h_hivpop(hm, ha, s);
          i_ha.h_deaths_excess_nonaids_agesex(ha, s) += opts.dt * deaths_excess_nonaids;
          n_ha.h_deaths_excess_nonaids_no_art(hm, ha, s) += opts.dt * deaths_excess_nonaids;

          i_ha.grad(hm, ha, s) = -(deaths_hiv + deaths_excess_nonaids);
        }

        for (int hm = 1; hm < hDS; ++hm) {
          const auto hiv_adults_progressing_cd4_stage = p_ha.cd4_progression(hm - 1, ha, s) * n_ha.h_hivpop(hm - 1, ha, s);
          i_ha.grad(hm - 1, ha, s) -= hiv_adults_progressing_cd4_stage;
          i_ha.grad(hm, ha, s) += hiv_adults_progressing_cd4_stage;
        }
      }
    }
  };

  void calc_new_infections_incidmod_transmrate(int hiv_step) {

    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;
    auto& n_dp = state_next.dp;
    auto& i_ha = intermediate.ha;

    // calculate HIV negative sexually active by age and sex
    nda::fill(i_ha.hiv_negative_pop, 0.0);
    for(int s = 0; s < NS; ++s) {
      for (int a = SS::p_idx_hiv_first_adult; a < pAG; ++a) {
        i_ha.hiv_negative_pop(a, s) = n_dp.p_totpop(a, s) - n_ha.p_hivpop(a, s);
      }

      if constexpr(ModelVariant::run_virgin) {
        // Remove virgin HIV negative from at risk population
        const auto& n_vg = state_next.vg;
        for (int va = 0, a = SS::p_idx_virginpop_first; va < SS::vAG; ++va, ++a) {
          i_ha.hiv_negative_pop(a, s) -= n_vg.p_totpop_virgin(va, s) - n_vg.p_hivpop_virgin(va, s);
        }
      }
    } // end s

    // sum population sizes
    real_type Xhivn_s[NS];
    real_type Xhivn_incagerr[NS];
    real_type Xhivp_noart = 0.0;
    real_type Xart = 0.0;

    for(int s = 0; s < NS; ++s){
      Xhivn_s[s] = 0.0;
      Xhivn_incagerr[s] = 0.0;
      for(int a = SS::pIDX_15to49; a < SS::pIDX_15to49 + SS::pAG_15to49; ++a) {
        Xhivn_s[s] += i_ha.hiv_negative_pop(a, s);
        Xhivn_incagerr[s] += p_ha.incidence_rate_ratio_age(a - p_ha.pIDX_INCIDPOP, s, t) * i_ha.hiv_negative_pop(a, s);
      }

      // Calculate ART coverage among age 15-49
      for(int ha = SS::hIDX_15to49; ha < SS::hIDX_15to49 + SS::hAG_15to49 + 1; ++ha){

        // adjustment to first and last age group for partial year time step
        // calculation proportion of HIV population to include / exclude based
        // on hivpop in single-year ages.
        real_type prop_include;
        if(ha == SS::hIDX_15to49){
          real_type hivp_ha = 0.0;
          int a = SS::pIDX_15to49;
          for(int i = 0; i < hAG_span[ha]; ++i, ++a) {
            hivp_ha += n_ha.p_hivpop(a, s);
          }
          prop_include = (hivp_ha > 0) ? 1.0 - n_ha.p_hivpop(SS::pIDX_15to49, s) / hivp_ha * (1.0 - opts.dt * hiv_step) : 1.0;
        } else if(ha == SS::hIDX_15to49 + SS::hAG_15to49) {
          real_type hivp_ha = 0.0;
          const int hAG_start_a = SS::pIDX_15to49 + SS::pAG_15to49;
          int a = hAG_start_a;
          for(int i = 0; i < hAG_span[ha]; ++i, ++a) {
            hivp_ha += n_ha.p_hivpop(a, s);
          }
          prop_include = (hivp_ha > 0) ? n_ha.p_hivpop(hAG_start_a, s) / hivp_ha * (1.0 - opts.dt * hiv_step) : 1.0;
        } else {
          prop_include = 1.0;
        }

        for(int hm = 0; hm < hDS; ++hm) {
          Xhivp_noart += n_ha.h_hivpop(hm, ha, s) * prop_include;
          if (t >= opts.ts_art_start) {
            for(int hu = 0; hu < hTS; ++hu) {
              Xart += n_ha.h_artpop(hu, hm, ha, s) * prop_include;
            }
          }
        }

      } // end loop over ha
    } // end loop over s

    real_type Xhivn = Xhivn_s[MALE] + Xhivn_s[FEMALE];

    // adjust HIV negative population for partial year time step
    for(int s = 0; s < NS; s++){
      Xhivn -= i_ha.hiv_negative_pop(SS::pIDX_15to49, s) * (1.0 - opts.dt * hiv_step);
      Xhivn += i_ha.hiv_negative_pop(SS::pIDX_15to49+SS::pAG_15to49, s) * (1.0 - opts.dt * hiv_step);
    }

    /*
      TO DO: Implement removal of virgin population from transmission rate model
    if constexpr(ModelVariant::run_virgin) {

      // Remove virgin population from HIV positive no-ART and ART populations
      const auto& n_vg = state_next.vg;

      const int vg_min_incid_age = std::max(0, SS::pIDX_15to49 - SS::p_idx_virginpop_first);
      const int vg_max_incid_age = std::min(SS::vAG, SS::pIDX_15to49 + SS::pAG_15to49 - SS::p_idx_virginpop_first);
      for (int va = vg_min_incid_age, a = vg_min_incid_age + SS::p_idx_virginpop_first;
        va < vg_max_incid_age; ++va, ++a) {

        // Remove virgin HIV negative from at risk population
        const auto Xhivn_virgin_sa = n_vg.p_totpop_virgin(va, s) - n_vg.p_hivpop_virgin(va, s);
        Xhivn_s[s] -= Xhivn_sa;
        Xhivn_incagerr[s] -= p_ha.incidence_rate_ratio_age(a - p_ha.pIDX_INCIDPOP, s, t) * Xhivn_virgin_sa;
      }
    }
    */

  real_type Xtot = Xhivn + Xhivp_noart + Xart;
  real_type prevcurr = (Xhivp_noart + Xart) / Xtot;

  int current_hiv_time_step = t * opts.hts_per_year + hiv_step;

  real_type incrate15to49_hts = p_ha.transmission_rate_hts[current_hiv_time_step] *
    (Xhivp_noart + p_ha.relative_infectiousness_art * Xart) / Xtot;

  // Seed incidence
  if (p_ha.epidemic_start_hts == current_hiv_time_step) {
    incrate15to49_hts += p_ha.initial_incidence;
  }

  // save HIV time step outputs
  n_ha.artcoverage_15to49_hts(hiv_step) = Xart + Xhivp_noart > 0.0 ? Xart / (Xart + Xhivp_noart) : 0.0;
  n_ha.prevalence_15to49_hts(hiv_step) = prevcurr;
  n_ha.incidence_15to49_hts(hiv_step) = incrate15to49_hts;

  // incidence by sex
  real_type incrate15to49_s[NS];
  incrate15to49_s[MALE] = incrate15to49_hts * (Xhivn_s[MALE]+Xhivn_s[FEMALE]) / (Xhivn_s[MALE] + p_ha.incidence_rate_ratio_sex(t)*Xhivn_s[FEMALE]);
  incrate15to49_s[FEMALE] = p_ha.incidence_rate_ratio_sex(t) * incrate15to49_s[MALE];

  // annualized infections by age and sex
  for(int s = 0; s < NS; ++s)
    for(int a = SS::p_idx_hiv_first_adult; a < pAG; a++){
      real_type hivn_a = n_dp.p_totpop(a, s) - n_ha.p_hivpop(a, s);
      i_ha.p_infections_ts(a, s) = hivn_a * incrate15to49_s[s] * p_ha.incidence_rate_ratio_age(a - SS::p_idx_hiv_first_adult, s, t) * Xhivn_s[s] / Xhivn_incagerr[s];
    }
  }

  void calc_new_infections_agesex() {
    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;
    auto& n_dp = state_next.dp;
    auto& i_ha = intermediate.ha;

    const auto adult_incid_first_age_group = p_ha.pIDX_INCIDPOP;
    const auto adult_incid_last_age_group = adult_incid_first_age_group + p_ha.pAG_INCIDPOP;

    // Calculate HIV infections by age. This uses the updated
    // 'current year' population [state_next] (vs. previous year
    // population used for overall incidence rate and incidence by sex)

    nda::fill(i_ha.hiv_negative_pop, 0.0);
    for (int s = 0; s < NS; ++s) {
      real_type Xhivn_incagerr = 0.0;

      for (int a = p_idx_hiv_first_adult; a < pAG; ++a) {
        i_ha.hiv_negative_pop(a, s) = n_dp.p_totpop(a, s) - n_ha.p_hivpop(a, s);
      }

      if constexpr(ModelVariant::run_virgin) {

        // Remove virgin HIV negative from at risk population
        const auto& n_vg = state_next.vg;

        for (int va = 0, a = SS::p_idx_virginpop_first; va < SS::vAG; ++va, ++a) {
          i_ha.hiv_negative_pop(a, s) -= n_vg.p_totpop_virgin(va, s) - n_vg.p_hivpop_virgin(va, s);
        }
      }

      for (int a = adult_incid_first_age_group; a < adult_incid_last_age_group; ++a) {
        Xhivn_incagerr += p_ha.incidence_rate_ratio_age(a - p_idx_hiv_first_adult, s, t) *
                          i_ha.hiv_negative_pop(a, s);
      }

      for (int a = SS::p_idx_hiv_first_adult; a < pAG; ++a) {
        i_ha.p_infections_ts(a, s) = i_ha.hiv_negative_pop(a, s) *
                                     i_ha.incidence_rate_sex(s) *
                                     p_ha.incidence_rate_ratio_age(a - p_idx_hiv_first_adult, s, t) *
                                     i_ha.hiv_neg_aggregate(s) /
                                     Xhivn_incagerr;
      }
    }
  };

  void calc_new_infections_agesex_goals() {
    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;
    auto& n_dp = state_next.dp;
    auto& i_ha = intermediate.ha;

    //map incidence from goals
    auto& n_hv = state_next.hv;

    const auto adult_incid_first_age_group = p_ha.pIDX_INCIDPOP;
    const auto adult_incid_last_age_group = adult_incid_first_age_group + p_ha.pAG_INCIDPOP;

    // Calculate HIV infections by age. This uses the updated
    // 'current year' population [state_next] (vs. previous year
    // population used for overall incidence rate and incidence by sex)

    for (int s = 0; s < NS; ++s) {

      nda::fill(i_ha.hiv_negative_pop, 0.0);
      real_type Xhivn_incagerr = 0.0;

      for (int a = adult_incid_first_age_group; a < pAG; ++a) {
        i_ha.hiv_negative_pop(a, s) = n_dp.p_totpop(a, s) - n_ha.p_hivpop(a, s);
      }

      for (int a = adult_incid_first_age_group; a < adult_incid_last_age_group; ++a) {
        Xhivn_incagerr += p_ha.incidence_rate_ratio_age(a - adult_incid_first_age_group, s, t) *
                          i_ha.hiv_negative_pop(a, s);
      }

      for (int a = SS::p_idx_hiv_first_adult; a < pAG; ++a) {
        i_ha.p_infections_ts(a, s) =  n_hv.new_infections_goals(s) * // new infections from goals
                                      i_ha.hiv_negative_pop(a, s) *
                                      p_ha.incidence_rate_ratio_age(a - adult_incid_first_age_group, s, t)/
                                      Xhivn_incagerr;
      }
    }
  };

  void add_new_hiv_infections() {
    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; s++) {
      int a = p_idx_hiv_first_adult;
      for (int ha = 0; ha < hAG; ++ha) {
        real_type p_infections_ha = 0.0;
        for (int i = 0; i < hAG_span[ha]; i++, a++) {
          const real_type p_infections_a = i_ha.p_infections_ts(a, s);
          p_infections_ha += p_infections_a;
          const auto new_infections = opts.dt * p_infections_a;
          n_ha.p_infections(a, s) += new_infections;
          n_ha.p_hivpop(a, s) += new_infections;

          if constexpr (ModelVariant::run_goals) {
            // Temporary, used for checking new infections updates in goals model
            if (i == 0 && SS::pIDX_15to49 <= a && a < SS::pIDX_15to49 + SS::pAG_15to49) {
                state_next.hv.new_infections_dp += new_infections;
            }
          }
        }

        // add p_infections to grad hivpop
        for (int hm = 0; hm < hDS; ++hm) {
          i_ha.grad_infections(hm, ha, s) += p_infections_ha * p_ha.cd4_initial_distribution(hm, ha, s);
        }
      }
    }
  };

  void run_art_progression_and_mortality() {
    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;


    for (int s = 0; s < NS; ++s) {
      for (int ha = 0; ha < hAG; ++ha) {
        for (int hm = i_ha.everARTelig_idx; hm < hDS; ++hm) {
          for (int hu = 0; hu < hTS; ++hu) {
            real_type deaths_art = p_ha.art_mortality(hu, hm, ha, s) *
                                   p_ha.art_mortality_time_rate_ratio(hu, t) *
                                   n_ha.h_artpop(hu, hm, ha, s);

           //capture the impact of AHD treament on art mortality
           if constexpr (ModelVariant::run_goals) {
              
              //impact of viral supression
              deaths_art *= intermediate.hv.alpha_mult;
       
              //impact of AHD intervention
              if ( (t > pars.hv.goals_base_year_idx) && (hm>=4) ) { // index 4 is CD4_100_199
                deaths_art *= intermediate.hv.AHD_Tx_Impact;
              }

              //impact of new products
              if ( (t > pars.hv.goals_base_year_idx)) { // all CD4 categories
                //impacts on art mortality, by risk group: functional cure
                deaths_art *= intermediate.hv.func_cure_impact_mort_all(s);
                //impacts on art mortality: therapeutic_vaccine
                deaths_art *= state_next.hv.prop_therapeutically_vaccinated(0, 1);
              }
            }

            const auto new_hiv_deaths_art = opts.dt * deaths_art;
            i_ha.h_hiv_deaths_age_sex(ha, s) += new_hiv_deaths_art;
            n_ha.h_hiv_deaths_art(hu, hm, ha, s) += new_hiv_deaths_art;

            const auto deaths_excess_nonaids = p_ha.art_nonaids_excess_mort(hu, hm, ha, s) * n_ha.h_artpop(hu, hm, ha, s);
            i_ha.h_deaths_excess_nonaids_agesex(ha, s) += opts.dt * deaths_excess_nonaids;
            n_ha.h_deaths_excess_nonaids_on_art(hu, hm, ha, s) += opts.dt * deaths_excess_nonaids;

            i_ha.gradART(hu, hm, ha, s) = -(deaths_art + deaths_excess_nonaids);
          }

          for (int hu = 0; hu < (hTS - 1); ++hu) {
            const auto art_adults_progressing_treatment_stage = n_ha.h_artpop(hu, hm, ha, s) / p_ha.h_art_stage_dur(hu);
            i_ha.gradART(hu, hm, ha, s) -= art_adults_progressing_treatment_stage;
            i_ha.gradART(hu + 1, hm, ha, s) += art_adults_progressing_treatment_stage;
          }

          // ART dropout
          if (p_ha.dropout_rate(t) > 0) {
            for (int hu = 0; hu < hTS; ++hu) {

              real_type temp_art_adult_dropout = 0.0;
              if constexpr (ModelVariant::run_goals) {
                if (t > pars.hv.goals_base_year_idx) {
                  temp_art_adult_dropout = -std::log(1.0 - pars.hv.art_interrupt_rate(t) *
                                                    (1.0 - pars.hv.long_act_treat_cov(t) * pars.hv.long_act_treat_eff_ltfu)) *
                                                    n_ha.h_artpop(hu, hm, ha, s);
                } else {
                  temp_art_adult_dropout = p_ha.dropout_rate(t) * n_ha.h_artpop(hu, hm, ha, s);
                }
              }
              else {
                temp_art_adult_dropout = p_ha.dropout_rate(t) * n_ha.h_artpop(hu, hm, ha, s);
              }

              const auto art_adult_dropout = temp_art_adult_dropout;

              if (p_ha.dropout_recover_cd4 && hu >= 2 && hm >= 1) {
                // recover people on ART >1 year to one higher CD4 category
                i_ha.grad(hm - 1, ha, s) += art_adult_dropout;
              } else {
                i_ha.grad(hm, ha, s) += art_adult_dropout;
              }
              i_ha.gradART(hu, hm, ha, s) -= art_adult_dropout;
            }
          }
        }
      }
    }
  };

  void run_h_art_initiation(int hiv_step) {
    const auto& p_ha = pars.ha;
    auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; ++s) {
      calc_art_eligibility(s);

      // How many people we aim to put on ART this time step.
      // There are 3 ART entry options at the moment
      // 1. Number or percent
      // 2. Initiation rate
      // 3. % by risk group - this is not implemented in AIM at the moment
      //   so we fallback to number or %.
      if (p_ha.art_entry_option == SS::ART_ENTRY_INITIATION_RATE) {
        i_ha.artinit_hts = calc_artinit_from_initiation_rate(s);
      } else {
        i_ha.artinit_hts = calc_artinit_from_number_or_percent(s, hiv_step);
      }

      // Spectrum ART allocation is a 2-step process
      // 1. Allocate by CD4 category (weighted by 'eligible' and 'expected mortality')
      // 2. Allocate by age groups (weighted only by eligibility)
      allocate_art_initiation_by_cd4();
      allocate_art_initiation_by_age(s);
    }
  };

  // Number eligible to start ART, their expected mortality, and the number
  // already on ART at the end of this time step. All ART entry options need
  // these to distribute initiations, and the number/percent option also needs
  // them to size the treatment gap.
  void calc_art_eligibility(int s) {
    const auto& p_ha = pars.ha;
    const auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    i_ha.Xart_15plus = 0.0;
    i_ha.Xartelig_15plus = 0.0;
    i_ha.expect_mort_artelig15plus = 0.0;
    nda::fill(i_ha.artelig_hm, 0.0);
    nda::fill(i_ha.expect_mort_artelig_hm, 0.0);

    const int cd4elig_idx = p_ha.idx_hm_elig(t);
    for (int ha = hIDX_15PLUS; ha < hAG; ++ha) {
      for (int hm = i_ha.everARTelig_idx; hm < hDS; ++hm) {
        if (hm >= i_ha.anyelig_idx) {
          // Special populations: below the CD4 threshold a share
          // specpop_percelig is eligible (with WHO stage 3/4 from stage 2)
          const real_type prop_elig = hm >= cd4elig_idx ? 1.0 :
            (hm >= 2 ? 1.0 - (1.0 - p_ha.specpop_percelig(t)) * (1.0 - p_ha.who34percelig) :
                       p_ha.specpop_percelig(t));
          const real_type artelig = prop_elig * n_ha.h_hivpop(hm, ha, s);
          i_ha.artelig_hahm(hm, ha - hIDX_15PLUS) = artelig;
          i_ha.artelig_hm(hm) += artelig;
          i_ha.Xartelig_15plus += artelig;

          const real_type expect_mort = p_ha.cd4_mortality(hm, ha, s) * artelig;
          i_ha.expect_mort_artelig_hm(hm) += expect_mort;
          i_ha.expect_mort_artelig15plus += expect_mort;
        }

        for (int hu = 0; hu < hTS; ++hu) {
          i_ha.Xart_15plus += n_ha.h_artpop(hu, hm, ha, s) +
                              opts.dt * i_ha.gradART(hu, hm, ha, s);
        }
      }

      // Pregnant women below the CD4 threshold (e.g. Option B+): in each stage
      // the eligible number is the age group's births times the FRR-weighted
      // HIV+ share of its women, as EPP's age model (AgeModel.run_hiv_model,
      // pw_artelig) and eppasm. Births are the year's, from before its HIV
      // deaths (pw_births_by_fertility_age).
      const int hf = ha - hIDX_15PLUS;
      if (s == FEMALE && p_ha.pw_artelig(t) > 0 && hf < hAG_fertility) {
        const auto& n_dp = state_next.dp;
        int a0 = p_idx_hiv_first_adult;
        for (int h = 0; h < ha; ++h) a0 += hAG_span[h];

        const real_type births_ha = i_ha.pw_births_by_fertility_age(hf);
        real_type frr_pop_ha = 0.0;
        for (int a = a0; a < a0 + hAG_span[ha]; ++a) {
          // HIV-negative women after this step's infections (EPP moves them
          // out of pop[t][0] before ART eligibility); hiv_negative_pop is
          // from before them. Virgin-stratified variants keep their own
          // at-risk definition.
          if constexpr (ModelVariant::run_virgin) {
            frr_pop_ha += i_ha.hiv_negative_pop(a, s);
          } else {
            frr_pop_ha += n_dp.p_totpop(a, s) - n_ha.p_hivpop(a, s);
          }
        }
        for (int hm = 0; hm < hDS; ++hm) {
          const real_type frr_cd4 = p_ha.local_adj_factor * p_ha.fert_mult_by_age(hf, t) *
                                    p_ha.fert_mult_off_art(hm);
          frr_pop_ha += frr_cd4 * n_ha.h_hivpop(hm, ha, s);
          for (int hu = 0; hu < hTS; ++hu) {
            const real_type frr_art = hu == 0 ? frr_cd4 :
              p_ha.local_adj_factor * p_ha.fert_mult_on_art(hf);
            frr_pop_ha += frr_art * n_ha.h_artpop(hu, hm, ha, s);
          }
        }
        if (frr_pop_ha > 0.0) {
          for (int hm = i_ha.anyelig_idx; hm < cd4elig_idx; ++hm) {
            const real_type frr_cd4 = p_ha.local_adj_factor * p_ha.fert_mult_by_age(hf, t) *
                                      p_ha.fert_mult_off_art(hm);
            const real_type pw_elig = births_ha * frr_cd4 * n_ha.h_hivpop(hm, ha, s) / frr_pop_ha;
            i_ha.artelig_hahm(hm, hf) += pw_elig;
            i_ha.artelig_hm(hm) += pw_elig;
            i_ha.Xartelig_15plus += pw_elig;
            const real_type expect_mort = p_ha.cd4_mortality(hm, ha, s) * pw_elig;
            i_ha.expect_mort_artelig_hm(hm) += expect_mort;
            i_ha.expect_mort_artelig15plus += expect_mort;
          }
        }
      }
    }
  };

  // ART entry option ART_ENTRY_INITIATION_RATE: initiations are a rate applied
  // to the treatment gap (PLHIV, plus this year's new infections, minus those
  // already on ART). Everything is read from the start-of-year state, matching
  // the way the rate was estimated.
  real_type calc_artinit_from_initiation_rate(int s) {
    const auto& p_ha = pars.ha;
    const auto& c_ha = state_curr.ha;

    real_type new_infections = 0.0;
    real_type plhiv = 0.0;
    for (int ha = 0; ha < hAG; ++ha) {
      const int a = ha + p_idx_hiv_first_adult;
      new_infections += c_ha.p_infections(a, s);
      plhiv += c_ha.p_hivpop(a, s);
    }

    const real_type treatment_gap = new_infections + plhiv;
    return std::max(opts.dt * p_ha.art_initiation_rate(s, t) * treatment_gap, 0.0);
  };

  // ART entry option ART_ENTRY_NUMBER_OR_PERCENT
  real_type calc_artinit_from_number_or_percent(int s, int hiv_step) {
    const auto& p_ha = pars.ha;
    auto& i_ha = intermediate.ha;

    real_type art_interp_w = opts.dt * (hiv_step + 1.0);
    real_type artnum_hts = 0.0;
    if (opts.proj_period_int == PROJPERIOD_MIDYEAR && art_interp_w < 0.5) {
      if (!p_ha.adults_on_art_is_percent(s, t - 2) && !p_ha.adults_on_art_is_percent(s, t - 1)) {
        // case: both values are numbers
        artnum_hts = (0.5 - art_interp_w) * p_ha.adults_on_art(s, t - 2) +
                     (art_interp_w + 0.5) * p_ha.adults_on_art(s, t - 1);
      } else if (p_ha.adults_on_art_is_percent(s, t - 2) && p_ha.adults_on_art_is_percent(s, t - 1)) {
        // case: both values are percentages
        const real_type artcov_hts = (0.5 - art_interp_w) * p_ha.adults_on_art(s, t - 2) +
                                     (art_interp_w + 0.5) * p_ha.adults_on_art(s, t - 1);
        artnum_hts = artcov_hts * (i_ha.Xart_15plus + i_ha.Xartelig_15plus);
      } else if (!p_ha.adults_on_art_is_percent(s, t - 2) && p_ha.adults_on_art_is_percent(s, t - 1)) {
        // case: value is percentage only at time t - 1
        // transition from number to percentage
        const real_type curr_coverage = i_ha.Xart_15plus / (i_ha.Xart_15plus + i_ha.Xartelig_15plus);
        const real_type artcov_hts = curr_coverage +
                                     (p_ha.adults_on_art(s, t - 1) - curr_coverage) *
                                     opts.dt / (0.5 - opts.dt * hiv_step);
        // back to number
        artnum_hts = artcov_hts * (i_ha.Xart_15plus + i_ha.Xartelig_15plus);
      }
    } else {
      // If the projection period is calendar year (>= Spectrum v6.2),
      // this condition is always followed, and it interpolates between
      // end of last year and current year (+ 1.0).
      // If projection period was mid-year (<= Spectrum v6.19), the second
      // half of the projection year interpolates the first half of the
      // calendar year (e.g. hts 7/10 for 2019 interpolates December 2018
      // to December 2019)

      if (opts.proj_period_int == PROJPERIOD_MIDYEAR) {
        art_interp_w -= 0.5;
      }

      if (!p_ha.adults_on_art_is_percent(s, t - 1) && !p_ha.adults_on_art_is_percent(s, t)) {
        // case: both values are numbers
        artnum_hts = (1.0 - art_interp_w) * p_ha.adults_on_art(s, t - 1) +
                     art_interp_w * p_ha.adults_on_art(s, t);
      } else if (p_ha.adults_on_art_is_percent(s, t - 1) && p_ha.adults_on_art_is_percent(s, t)) {
        // case: both values are percentages
        const real_type artcov_hts = (1.0 - art_interp_w) * p_ha.adults_on_art(s, t - 1) +
                                     art_interp_w * p_ha.adults_on_art(s, t);
        // transition to number
        artnum_hts = artcov_hts * (i_ha.Xart_15plus + i_ha.Xartelig_15plus);
      } else if (!p_ha.adults_on_art_is_percent(s, t - 1) && p_ha.adults_on_art_is_percent(s, t)) {
        // case: value is percentage only at time t
        // transition from number to percentage
        const real_type curr_coverage = i_ha.Xart_15plus / (i_ha.Xart_15plus + i_ha.Xartelig_15plus);
        const real_type artcov_hts = curr_coverage +
                                     (p_ha.adults_on_art(s, t) - curr_coverage) *
                                     opts.dt / (1.0 - art_interp_w + opts.dt);
        // back to number
        artnum_hts = artcov_hts * (i_ha.Xart_15plus + i_ha.Xartelig_15plus);
      }
    }

    return std::max(artnum_hts - i_ha.Xart_15plus, 0.0);
  };

  // Step 1: allocate ART initiations by CD4 stage, weighting eligibility against
  // expected mortality
  // Distribute total initiations over CD4 stages by the given weights; a stage cannot take more than its eligible
  // population at the start of the step, and what it cannot take passes to the others by their weights
  void fill_cd4_stages(const real_type* weight, real_type total, real_type* alloc) {
    auto& i_ha = intermediate.ha;
    bool open[hDS] = {false};
    for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) { alloc[hm] = 0.0; open[hm] = i_ha.artelig_hm(hm) > 0.0; }
    real_type remaining = total;
    for (int iter = 0; iter < hDS && remaining > 0.0; ++iter) {
      real_type wsum = 0.0;
      for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) if (open[hm]) wsum += weight[hm];
      if (wsum <= 0.0) break;
      bool capped = false;
      for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) {
        if (open[hm] && alloc[hm] + remaining * weight[hm] / wsum >= i_ha.artelig_hm(hm)) {
          remaining -= i_ha.artelig_hm(hm) - alloc[hm];
          alloc[hm] = i_ha.artelig_hm(hm);
          open[hm] = false;
          capped = true;
        }
      }
      if (!capped) {
        for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) if (open[hm]) alloc[hm] += remaining * weight[hm] / wsum;
        remaining = 0.0;
      }
    }
  }

  // Each scheme (by eligibility, by expected mortality) allocates all initiations on its own, with the stage caps,
  // and the result is their weighted average (AIM: "the average of two allocation schemes"). Spectrum 6.43 MWI: with
  // the shares mixed before allocating, untreated women at CD4 < 50 ran to 0.18-0.89 of Spectrum's from 2013; with
  // the allocations averaged they are within 1% in every year
  //
  // initiation_allocation_separate = 0 mixes the two shares and allocates once, with no stage cap (eppasm, and EPP's
  // age model: AgeModel.run_hiv_model, art_alloc_method other than 4); the cap then acts per age group in
  // allocate_art_initiation_by_age()
  void allocate_art_initiation_by_cd4() {
    const auto& p_ha = pars.ha;
    auto& i_ha = intermediate.ha;
    if (p_ha.initiation_allocation_separate != 1) {
      for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) {
        real_type eligibility_by_stage = 0.0;
        if (i_ha.Xartelig_15plus > 0.0) {
          eligibility_by_stage = (1.0 - p_ha.initiation_mortality_weight) * i_ha.artelig_hm(hm) / i_ha.Xartelig_15plus;
        }
        real_type expected_mortality_by_stage = 0.0;
        if (i_ha.expect_mort_artelig15plus > 0.0) {
          expected_mortality_by_stage = p_ha.initiation_mortality_weight * i_ha.expect_mort_artelig_hm(hm) /
                                        i_ha.expect_mort_artelig15plus;
        }
        i_ha.artinit_hm(hm) = i_ha.artinit_hts * (eligibility_by_stage + expected_mortality_by_stage);
      }
      return;
    }
    real_type w_elig[hDS] = {0.0}, w_mort[hDS] = {0.0}, a_elig[hDS] = {0.0}, a_mort[hDS] = {0.0};
    for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) {
      w_elig[hm] = i_ha.artelig_hm(hm);
      w_mort[hm] = i_ha.expect_mort_artelig_hm(hm);
    }
    fill_cd4_stages(w_elig, i_ha.artinit_hts, a_elig);
    fill_cd4_stages(w_mort, i_ha.artinit_hts, a_mort);
    for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) {
      i_ha.artinit_hm(hm) = (1.0 - p_ha.initiation_mortality_weight) * a_elig[hm] +
                            p_ha.initiation_mortality_weight * a_mort[hm];
    }
  };

  // Step 2: within CD4 stage, allocate ART initiations by age proportional to
  // eligibility, capped by the population actually available to initiate
  void allocate_art_initiation_by_age(int s) {
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    for (int ha = hIDX_15PLUS; ha < hAG; ++ha) {
      for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) {
        if (i_ha.artelig_hm(hm) > 0.0) {
          real_type artinit_hahm = i_ha.artinit_hm(hm) *
                                   i_ha.artelig_hahm(hm, ha - hIDX_15PLUS) /
                                   i_ha.artelig_hm(hm);
          artinit_hahm = std::min(artinit_hahm, i_ha.artelig_hahm(hm, ha - hIDX_15PLUS));
          artinit_hahm = std::min(artinit_hahm,
                                  n_ha.h_hivpop(hm, ha, s) + opts.dt * i_ha.grad(hm, ha, s));
          i_ha.grad(hm, ha, s) -= artinit_hahm / opts.dt;
          i_ha.gradART(ART0MOS, hm, ha, s) += artinit_hahm / opts.dt;
          n_ha.h_art_initiation(hm, ha, s) += artinit_hahm;
        }
      }
    }
  };

  void run_update_art_adult() {
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; ++s) {
      for (int ha = 0; ha < hAG; ++ha) {
        for (int hm = i_ha.everARTelig_idx; hm < hDS; ++hm) {
          for (int hu = 0; hu < hTS; ++hu) {
            n_ha.h_artpop(hu, hm, ha, s) += opts.dt * i_ha.gradART(hu, hm, ha, s);
          }
        }
      }
    }
  };

  void run_update_hiv_adult() {
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; ++s) {
      for (int ha = 0; ha < hAG; ++ha) {
        for (int hm = 0; hm < hDS; ++hm) {
          n_ha.h_hivpop(hm, ha, s) += opts.dt * (i_ha.grad_infections(hm, ha, s) + i_ha.grad(hm, ha, s));
        }
      }
    }
  };

  void run_calc_p_hiv_deaths() {
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; ++s) {
      // sum HIV+ population size in each hivpop age group
      int a = p_idx_hiv_first_adult;
      for (int ha = 0; ha < hAG; ++ha) {
        i_ha.hivpop_ha(ha) = 0.0;
        for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
          i_ha.hivpop_ha(ha) += n_ha.p_hivpop(a, s);
        }
      }

      // remove hivdeaths proportionally to age-distribution within each age group
      a = p_idx_hiv_first_adult;
      for (int ha = 0; ha < hAG; ++ha) {

        if (i_ha.hivpop_ha(ha) > 0) {
          const real_type hivqx_ha = i_ha.h_hiv_deaths_age_sex(ha, s) / i_ha.hivpop_ha(ha);
          auto nonaids_excess_qx_ha = i_ha.h_deaths_excess_nonaids_agesex(ha, s) / i_ha.hivpop_ha(ha);

          for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
            auto deaths_aids_a = n_ha.p_hivpop(a, s) * hivqx_ha;
            auto deaths_nonaids_excess_a = n_ha.p_hivpop(a, s) * nonaids_excess_qx_ha;

            i_ha.deaths_hivpop_hts(a, s) = deaths_aids_a + deaths_nonaids_excess_a;

            n_ha.p_hiv_deaths(a, s) += deaths_aids_a;
            n_ha.p_deaths_excess_nonaids(a, s) += deaths_nonaids_excess_a;
          }

        } else {
          for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
            i_ha.deaths_hivpop_hts(a, s) = 0.0;
          }
        }
      }
    }

  };

  void run_remove_p_hiv_deaths() {
    auto& i_ha = intermediate.ha;
    auto& n_dp = state_next.dp;
    auto& n_ha = state_next.ha;
    for (int s = 0; s < NS; ++s) {
      for (int a = p_idx_hiv_first_adult; a < pAG; ++a) {
        n_dp.p_totpop(a, s) -= i_ha.deaths_hivpop_hts(a, s);
        n_ha.p_hivpop(a, s) -= i_ha.deaths_hivpop_hts(a, s);
      }
    }
  }

  void run_update_hiv_virgin() {

    // Implements progression, HIV mortality, and ART initiation in the
    // virgin population proportional to the proportion of the total population
    // that is virgin.
    // Imposes the assumtion that same rates apply in the virgin population as
    // in the adult population.

    // Current implementation assumes h_hivpop_virgin stratification aligns
    // to single-year age groups ("full stratification" rather than "coarse").
    // Code requires updating to handle coarse virgin pop age groups.

    const auto& n_ha = state_next.ha;
    const auto& i_ha = intermediate.ha;
    auto& n_vg = state_next.vg;

    for (int s = 0; s < NS; ++s) {
      for (int va = 0; va < SS::vAG; ++va) {
        for (int hm = 0; hm < hDS; ++hm) {
          auto prop_virgin = n_vg.h_hivpop_virgin(hm, va, s) / n_ha.h_hivpop(hm, va, s);
          n_vg.h_hivpop_virgin(hm, va, s) += opts.dt * i_ha.grad(hm, va, s) * prop_virgin;
        }
      }
    }
  };

  void run_update_art_virgin() {
    const auto& n_ha = state_next.ha;
    const auto& i_ha = intermediate.ha;
    auto& n_vg = state_next.vg;

    for (int s = 0; s < NS; ++s) {
      for (int va = 0; va < SS::vAG; ++va) {
        for (int hm = i_ha.everARTelig_idx; hm < hDS; ++hm) {
          for (int hu = 0; hu < hTS; ++hu) {
            auto prop_virgin = n_vg.h_artpop_virgin(hu, hm, va, s) / n_ha.h_artpop(hu, hm, va, s);
            n_vg.h_artpop_virgin(hu, hm, va, s) += opts.dt * i_ha.gradART(hu, hm, va, s) * prop_virgin;
          }
        }
      }
    }
  };

  void run_remove_p_virgin_hiv_deaths() {
    const auto& i_ha = intermediate.ha;
    const auto& n_ha = state_next.ha;
    auto& n_vg = state_next.vg;
    for (int s = 0; s < NS; ++s) {
      for (int va = 0; va < SS::vAG; ++va) {
        int a = va + SS::p_idx_virginpop_first; // age index in total population array
        if (n_ha.p_hivpop(a, s) > 0.0) {
          auto deaths_virginpop_a = i_ha.deaths_hivpop_hts(a, s) * n_vg.p_hivpop_virgin(va, s) / n_ha.p_hivpop(a, s);
          n_vg.p_totpop_virgin(va, s) -= deaths_virginpop_a;
          n_vg.p_hivpop_virgin(va, s) -= deaths_virginpop_a;
        }
      }
    }
  };

  void run_wlhiv_births() {
    const auto& p_dp = pars.dp;
    const auto& p_ha = pars.ha;
    const auto& c_ha = state_curr.ha;
    auto& n_ha = state_next.ha;
    auto& n_dp = state_next.dp;
    real_type asfr_sum = 0.0;
    for (int a = 0; a < p_fertility_age_groups; ++a) {
      asfr_sum += p_dp.age_specific_fertility_rate(a, t);
    } // end a

    int a_idx_in = p_idx_fertility_first;
    n_ha.hiv_births = 0.0;
    for (int ha = 0; ha < hAG_fertility; ++ha) {
      real_type nHIVcurr = 0.0;
      real_type nHIVlast = 0.0;
      real_type df = 0.0;

      for (int hd = 0; hd < hDS; ++hd) {
        nHIVcurr += n_ha.h_hivpop(hd, ha, FEMALE);
        nHIVlast += c_ha.h_hivpop(hd, ha, FEMALE);
        for (int ht = 0; ht < hTS; ++ht) {
          nHIVcurr += n_ha.h_artpop(ht, hd, ha, FEMALE);
          nHIVlast += c_ha.h_artpop(ht, hd, ha, FEMALE);
        } // end hTS
      } // end hDS

      auto total_pop = 0.0;
      auto asfr_w = 0.0;
      for (int a_idx = a_idx_in; a_idx < (a_idx_in + hAG_span[ha]); ++a_idx) {
        total_pop += n_dp.p_totpop(a_idx, FEMALE);
        asfr_w += p_dp.age_specific_fertility_rate(a_idx - p_idx_fertility_first, t) / asfr_sum;
      }
      //set up a_idx_in for the next loop
      a_idx_in = a_idx_in + hAG_span[ha];
      asfr_w /= hAG_span[ha];

      const real_type prev = nHIVcurr / total_pop;

      for (int hd = 0; hd < hDS; ++hd) {
        df += p_ha.local_adj_factor *
          p_ha.fert_mult_by_age(ha, t) *
          p_ha.fert_mult_off_art(hd) *
          (n_ha.h_hivpop(hd, ha, FEMALE) + c_ha.h_hivpop(hd, ha, FEMALE)) / 2;

        // women on ART less than 6 months use the off art fertility multiplier
        df += p_ha.local_adj_factor *
          p_ha.fert_mult_by_age(ha, t) *
          p_ha.fert_mult_off_art(hd) *
          (n_ha.h_artpop(0, hd, ha, FEMALE) + c_ha.h_artpop(0, hd, ha, FEMALE)) / 2;
        for (int ht = 1; ht < hTS; ++ht) {
          df += p_ha.local_adj_factor *
            p_ha.fert_mult_on_art(ha) *
            (n_ha.h_artpop(ht, hd, ha, FEMALE) + c_ha.h_artpop(ht, hd, ha, FEMALE)) / 2;
        } // end hTS
      } // end hDS

      auto midyear_fertileHIV = (nHIVcurr + nHIVlast) / 2;
      if (midyear_fertileHIV > 0) {
        df = df / midyear_fertileHIV;
      } else {
        df = 1;
      }

      n_ha.hiv_births_by_mat_age(ha) = midyear_fertileHIV * p_dp.total_fertility_rate(t) *
        df / (df * prev + 1 - prev) *
        asfr_w;


      n_ha.hiv_births += n_ha.hiv_births_by_mat_age(ha);
    } // end ha

    if constexpr (ModelVariant::run_goals) {
      if (t > pars.hv.goals_base_year_idx) {
        n_ha.hiv_births *= (1-pars.hv.rn_cure_coverage_neonates(t)*pars.hv.rn_cure_effect_neonates);
      }
    }
  };

  void apply_goals_cure_adults() {
    auto& n_ha = state_next.ha;
    const auto& i_ha = intermediate.ha;

    for (int s = 0; s < NS; ++s) {
      // cure cov includes adjustment for the proportion already received cured over period of duration
      const real_type cure_cov = intermediate.hv.cure_avg_cov_adults(s);
      const real_type cure_cov_impact = intermediate.hv.cure_avg_cov_adults_impact(s);
      real_type cured = 0.0;

      for (int ha = 0; ha < hAG; ++ha) {
        const int a = ha + p_idx_hiv_first_adult;

        // adults, PLHIV not on ART
        for (int hm = 0; hm < hDS; ++hm) {

          if(a >= SS::pIDX_15to49 + SS::pAG_15to49){
            // 15-49 new_cures tracked in goals
            // for costing, use proportion for costing, without efficacy applied
            cured = cure_cov * n_ha.h_hivpop(hm, ha, s);

            // do not remove more than 99 % of the current compartment
            cured = std::min(cured, 0.99 * n_ha.h_hivpop(hm, ha, s));

            // add to total for costing
            state_next.hv.total_new_cures += cured;
           }

          //for impact, use proportion for costing, with efficacy applied
          cured = cure_cov_impact * n_ha.h_hivpop(hm, ha, s);

          // do not remove more than 99 % of the current compartment
          cured = std::min(cured, 0.99 * n_ha.h_hivpop(hm, ha, s));

          n_ha.h_hivpop(hm, ha, s) -= cured;
          n_ha.p_hivpop(a, s) -= cured;

        }

         // adults, PLHIV on ART
        for (int hm = i_ha.everARTelig_idx; hm < hDS; ++hm) {
          for (int hu = 0; hu < hTS; ++hu) {

          if(a >= SS::pIDX_15to49 + SS::pAG_15to49){
            // 15-49 new_cures tracked in goals
            // for costing, use proportion for costing, without efficacy applied
            cured = cure_cov * n_ha.h_artpop(hu, hm, ha, s);

            // do not remove more than 99 % of the current compartment
            cured = std::min(cured, 0.99 * n_ha.h_artpop(hu, hm, ha, s));

            // add to total for costing
            state_next.hv.total_new_cures += cured;
          }

          // for impact, use proportion for costing, with efficacy applied
          cured = cure_cov_impact * n_ha.h_artpop(hu, hm, ha, s);

          // do not remove more than 99 % of the current compartment
          cured = std::min(cured, 0.99 * n_ha.h_artpop(hu, hm, ha, s));

          n_ha.h_artpop(hu, hm, ha, s) -= cured;
          n_ha.p_hivpop(a, s) -= cured;

          }
        }

      } // ha
    } // s
  };


};

}
}
