#pragma once

#include "../options.hpp"
#include "../generated/config_mixer.hpp"

namespace leapfrog {
namespace internal {

template<typename Config>
concept HivDemographicProjectionEnabled = RunDemographicProjection<Config> && RunHivSimulation<Config>;

template<typename Config>
struct HivDemographicProjection {
  HivDemographicProjection(...) {};
};

template<HivDemographicProjectionEnabled Config>
struct HivDemographicProjection<Config> {
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
  static constexpr int p_idx_hiv_first_adult = SS::p_idx_hiv_first_adult;
  static constexpr int FEMALE = SS::FEMALE;
  static constexpr int ART0MOS = SS::ART0MOS;
  static constexpr int hAG_fertility = SS::hAG_fertility;
  static constexpr int p_idx_fertility_first = SS::p_idx_fertility_first;


  // function args
  int t;
  const Pars& pars;
  const State& state_curr;
  State& state_next;
  Intermediate& intermediate;
  const Options<real_type>& opts;

  // only exposing the constructor and some methods
  public:
  HivDemographicProjection(Args& args):
    t(args.t),
    pars(args.pars),
    state_curr(args.state_curr),
    state_next(args.state_next),
    intermediate(args.intermediate),
    opts(args.opts)
  {};

  void run_hivpop_demographic_projection() {

    run_hiv_ageing_and_mortality();
    if constexpr (ModelVariant::run_child_model) {
      run_age_15_entrants();
    }
    run_hiv_and_art_stratified_ageing();
    if constexpr (ModelVariant::run_static_entrants) {
      run_static_age_15_entrants();
      run_static_entrants_births_by_fertility_age();
    }
    run_hiv_and_art_stratified_deaths_and_migration();
    if constexpr (ModelVariant::run_child_model) {
      run_hc_hiv_and_art_stratified_deaths_and_migration();
    }

  };

  // Births-weighted HIV prevalence among pregnant women at the end of the year
  // as EPP's age model computes it (pregprev), kept for 15 years so that this
  // year's value reaches the age-15 entrants 15 years on. Within each fertile
  // age group, HIV+ women weighted by their fertility ratios (untreated and on
  // ART under 6 months: local_adj_factor * fert_mult_by_age * fert_mult_off_art;
  // on ART longer: local_adj_factor * fert_mult_on_art), averaged over the
  // year, against the averaged HIV-negative women.
  void run_static_entrants_pregprev() {
    const auto& p_ha = pars.ha;
    const auto& c_dp = state_curr.dp;
    const auto& n_dp = state_next.dp;
    const auto& c_ha = state_curr.ha;
    const auto& n_ha = state_next.ha;
    const auto& i_en = intermediate.en;
    const auto& c_en = state_curr.en;
    auto& n_en = state_next.en;

    real_type births = 0.0;
    real_type hiv_births = 0.0;
    int a = p_idx_fertility_first;
    for (int ha = 0; ha < hAG_fertility; ++ha) {
      real_type hivn = 0.0;
      for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
        hivn += (c_dp.p_totpop(a, FEMALE) - c_ha.p_hivpop(a, FEMALE) +
                 n_dp.p_totpop(a, FEMALE) - n_ha.p_hivpop(a, FEMALE)) / 2.0;
      }
      real_type frr_hivpop = 0.0;
      for (int hd = 0; hd < hDS; ++hd) {
        const real_type frr_off_art = p_ha.local_adj_factor * p_ha.fert_mult_by_age(ha, t) * p_ha.fert_mult_off_art(hd);
        frr_hivpop += frr_off_art * (c_ha.h_hivpop(hd, ha, FEMALE) + n_ha.h_hivpop(hd, ha, FEMALE)) / 2.0;
        frr_hivpop += frr_off_art * (c_ha.h_artpop(ART0MOS, hd, ha, FEMALE) + n_ha.h_artpop(ART0MOS, hd, ha, FEMALE)) / 2.0;
        for (int ht = ART0MOS + 1; ht < hTS; ++ht) {
          frr_hivpop += p_ha.local_adj_factor * p_ha.fert_mult_on_art(ha) *
            (c_ha.h_artpop(ht, hd, ha, FEMALE) + n_ha.h_artpop(ht, hd, ha, FEMALE)) / 2.0;
        }
      }
      births += i_en.births_by_fertility_age(ha);
      if (hivn + frr_hivpop > 0.0) {
        hiv_births += i_en.births_by_fertility_age(ha) * frr_hivpop / (hivn + frr_hivpop);
      }
    }

    for (int k = p_idx_hiv_first_adult - 1; k > 0; --k) {
      n_en.pregprev_lag(k) = c_en.pregprev_lag(k - 1);
    }
    n_en.pregprev_lag(0) = births > 0.0 ? hiv_births / births : 0.0;
  };

  void run_hivpop_end_year_migration() {
    auto& n_ha = state_next.ha;
    auto& i_dp = intermediate.dp;

    // remove net migration from hiv stratified population
    for (int s = 0; s < NS; ++s) {
      for (int a = 0; a < pAG; ++a) {
        n_ha.p_net_migration_hivpop(a, s) = n_ha.p_hivpop(a, s) * i_dp.migration_rate(a, s);
        n_ha.p_hivpop(a, s) += n_ha.p_net_migration_hivpop(a, s);
      }
    }

    // remove net migration from adult stratified population
    for (int s = 0; s < NS; ++s) {
      int a = p_idx_hiv_first_adult;
      for (int ha = 0; ha < hAG; ++ha) {
        real_type migration_num_ha = 0.0;
        real_type hivpop_ha_postmig = 0.0;
        for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
          hivpop_ha_postmig += n_ha.p_hivpop(a, s);
          migration_num_ha += n_ha.p_net_migration_hivpop(a, s);
        }

        real_type migration_rate = 0.0;
        if (hivpop_ha_postmig > 0.0) {
          migration_rate = migration_num_ha / (hivpop_ha_postmig - migration_num_ha);
        }

        for (int hm = 0; hm < hDS; ++hm) {
          n_ha.h_hivpop(hm, ha, s) *= 1.0 + migration_rate;
          if (t >= opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              n_ha.h_artpop(hu, hm, ha, s) *= 1.0 + migration_rate;
            }
          }
        }
      }
    }
  };

  void run_hivpop_population_adjustment() {
    static_assert(ModelVariant::run_population_adjustment,
                  "run_hivpop_population_adjustment can only be called for model variants where run_population_adjustment is true");
    static_assert(!ModelVariant::run_virgin,
                  "population adjustment does not yet scale the virgin population");
    auto& n_ha = state_next.ha;
    const auto& i_pa = intermediate.pa;

    // Adult stratified population: scale each HIV age group by the change in
    // its single-age HIV population, as for migration
    for (int s = 0; s < NS; ++s) {
      int a = p_idx_hiv_first_adult;
      for (int ha = 0; ha < hAG; ++ha) {
        real_type hivpop_ha = 0.0;
        real_type hivpop_ha_adj = 0.0;
        for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
          hivpop_ha += n_ha.p_hivpop(a, s);
          hivpop_ha_adj += n_ha.p_hivpop(a, s) * i_pa.population_adjustment_ratio(a, s);
        }

        real_type adjustment_ratio = hivpop_ha > 0.0 ? hivpop_ha_adj / hivpop_ha : 1.0;
        for (int hm = 0; hm < hDS; ++hm) {
          n_ha.h_hivpop(hm, ha, s) *= adjustment_ratio;
          if (t >= opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              n_ha.h_artpop(hu, hm, ha, s) *= adjustment_ratio;
            }
          }
        }
      }
    }

    for (int s = 0; s < NS; ++s) {
      for (int a = 0; a < pAG; ++a) {
        n_ha.p_hivpop(a, s) *= i_pa.population_adjustment_ratio(a, s);
      }
    }
  };

  void run_hc_hivpop_population_adjustment() {
    static_assert(ModelVariant::run_child_model && ModelVariant::run_population_adjustment,
                  "run_hc_hivpop_population_adjustment can only be called for model variants where run_child_model and run_population_adjustment are true");
    static constexpr int hc2_agestart = SS::hc2_agestart;
    static constexpr int hcAG_end = SS::hcAG_end;
    static constexpr int hc1DS = SS::hc1DS;
    static constexpr int hc2DS = SS::hc2DS;
    static constexpr int hcTT = SS::hcTT;

    auto& n_hc = state_next.hc;
    const auto& p_hc = pars.hc;
    const auto& i_pa = intermediate.pa;

    for (int s = 0; s < NS; ++s) {
      for (int a = 0; a < hcAG_end; ++a) {
        real_type adjustment_ratio = i_pa.population_adjustment_ratio(a, s);
        if (a < hc2_agestart) {
          for (int cat = 0; cat < hcTT; ++cat) {
            for (int hd = 0; hd < hc1DS; ++hd) {
              n_hc.hc1_hivpop(hd, cat, a, s) *= adjustment_ratio;
            }
          }
          if (t >= p_hc.hc_art_start) {
            for (int hd = 0; hd < hc1DS; ++hd) {
              for (int dur = 0; dur < hTS; ++dur) {
                n_hc.hc1_artpop(dur, hd, a, s) *= adjustment_ratio;
              }
            }
          }
        } else {
          for (int hd = 0; hd < hc2DS; ++hd) {
            for (int cat = 0; cat < hcTT; ++cat) {
              n_hc.hc2_hivpop(hd, cat, a - hc2_agestart, s) *= adjustment_ratio;
            }
            if (t >= p_hc.hc_art_start) {
              for (int dur = 0; dur < hTS; ++dur) {
                n_hc.hc2_artpop(dur, hd, a - hc2_agestart, s) *= adjustment_ratio;
              }
            }
          }
        }
      }
    }
  };

  void run_hc_hivpop_end_year_migration() {
    static_assert(ModelVariant::run_child_model,
                  "run_hc_hivpop_end_year_migration can only be called for model variants where run_child_model is true");
    static constexpr int hc2_agestart = SS::hc2_agestart;
    static constexpr int hcAG_end = SS::hcAG_end;
    static constexpr int hc1DS = SS::hc1DS;
    static constexpr int hc2DS = SS::hc2DS;
    static constexpr int hTS = SS::hTS;
    static constexpr int hcTT = SS::hcTT;

    auto& n_ha = state_next.ha;
    auto& n_hc = state_next.hc;
    const auto& p_hc = pars.hc;

    for (int s = 0; s < NS; ++s) {
      for (int a = 0; a < hcAG_end; ++a) {
        real_type hc_migration_num = 0.0;
        real_type hc_hivpop_postmig = n_ha.p_hivpop(a, s);
        hc_migration_num = n_ha.p_net_migration_hivpop(a, s);

        real_type migration_rate = 0.0;
        if (hc_hivpop_postmig > 0.0) {
          migration_rate = hc_migration_num / (hc_hivpop_postmig - hc_migration_num);
        }

        if (a < hc2_agestart) {
          for (int cat = 0; cat < hcTT; ++cat) {
            for (int hd = 0; hd < hc1DS; ++hd) {
              n_hc.hc1_hivpop(hd, cat, a, s) *= 1.0 + migration_rate;
            }
          }
          if (t >= p_hc.hc_art_start) {
            for (int hd = 0; hd < hc1DS; ++hd) {
              for (int dur = 0; dur < hTS; ++dur) {
                n_hc.hc1_artpop(dur, hd, a, s) *= 1.0 + migration_rate;
              }
            }
          }
        } else {
          for (int hd = 0; hd < hc2DS; ++hd) {
            for (int cat = 0; cat < hcTT; ++cat) {
              n_hc.hc2_hivpop(hd, cat, a - hc2_agestart, s) *= 1.0 + migration_rate;
            }
            if (t >= p_hc.hc_art_start) {
              for (int dur = 0; dur < hTS; ++dur) {
                n_hc.hc2_artpop(dur, hd, a - hc2_agestart, s) *= 1.0 + migration_rate;
              }
            }
          }
        }
      }
    }

  };

  void run_virginpop_demographic_projection() {
    const auto& p_dp = pars.dp;
    const auto& c_dp = state_curr.dp;
    const auto& i_dp = intermediate.dp;
    const auto& c_ha = state_curr.ha;
    const auto& c_vg = state_curr.vg;
    const auto& i_hc = intermediate.hc;
    const auto& p_hc = pars.hc;
    const auto& p_vg = pars.vg;
    auto& n_vg = state_next.vg;

    // Non-hiv deaths and ageing

    for (int s = 0; s < NS; ++s) {

      // Ageing and survival of virgin population
      for (int va = 1; va < SS::vAG; ++va) {

        int a = va + SS::p_idx_virginpop_first; // age index in total population array
        n_vg.p_totpop_virgin(va, s) = c_vg.p_totpop_virgin(va - 1, s) * p_dp.survival_probability(a, s, t); // virgin total population
        n_vg.p_hivpop_virgin(va, s) = c_vg.p_hivpop_virgin(va - 1, s) * p_dp.survival_probability(a, s, t); // virgin HIV positive population

        // Ageing virgin HIV stratified population
        // Note: code relies on virgin HIV population being single-age stratified
        for (int hm = 0; hm < hDS; ++hm) {
          n_vg.h_hivpop_virgin(hm, va, s) += c_vg.h_hivpop_virgin(hm, va - 1, s) * p_dp.survival_probability(a, s, t);

          if (t > opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              n_vg.h_artpop_virgin(hu, hm, va, s) += c_vg.h_artpop_virgin(hu, hm, va - 1, s) * p_dp.survival_probability(a, s, t);
            }
          }
        }

        if (opts.proj_period_int == PROJPERIOD_MIDYEAR) {
          n_vg.p_totpop_virgin(va, s) *= 1.0 + i_dp.migration_rate(a, s);
          n_vg.p_hivpop_virgin(va, s) *= 1.0 + i_dp.migration_rate(a, s);

          // Migration virgin HIV stratified population
          // Note: code relies on virgin HIV population being single-age stratified
          for (int hm = 0; hm < hDS; ++hm) {
            n_vg.h_hivpop_virgin(hm, va, s) *= 1.0 + i_dp.migration_rate(a, s);

            if (t > opts.ts_art_start) {
              for (int hu = 0; hu < hTS; ++hu) {
                n_vg.h_artpop_virgin(hu, hm, va, s) *= 1.0 + i_dp.migration_rate(a, s);
              }
            }
          }
        }
      }

      // Entrants into virgin population from total population age (p_idx_virginpop_first - 1)
      const int va0 = 0;
      const int a0 = SS::p_idx_virginpop_first;
      n_vg.p_totpop_virgin(va0, s) = c_dp.p_totpop(a0 - 1, s) * p_dp.survival_probability(a0, s, t);
      n_vg.p_hivpop_virgin(va0, s) = c_ha.p_hivpop(a0 - 1, s) * p_dp.survival_probability(a0, s, t);

      // Entrants into virgin HIV stratified population
      // Note: code relies on virgin HIV population being single-age stratified; needs
      // to be generalised if implementing coarse HIV population.
      for (int hm = 0; hm < hDS; ++hm) {
        n_vg.h_hivpop_virgin(hm, va0, s) += i_hc.age15_hivpop(hm, s) * p_dp.survival_probability(a0, s, t);

        if (t > p_hc.hc_art_start) {
          for (int hu = 0; hu < hTS; ++hu) {
            if (t > opts.ts_art_start) {
              n_vg.h_artpop_virgin(hu, hm, va0, s) += i_hc.age15_artpop(hu, hm, s) * p_dp.survival_probability(a0, s, t);
            } else {
              // If child ART has started, but not yet adult ART has started, put
              // children on ART into the untreated adult HIV population
              n_vg.h_hivpop_virgin(hm, va0, s) += i_hc.age15_artpop(hu, hm, s) * p_dp.survival_probability(a0, s, t);
            }
          }
        }
      }

      if (opts.proj_period_int == PROJPERIOD_MIDYEAR) {
        n_vg.p_totpop_virgin(va0, s) *= 1.0 + i_dp.migration_rate(a0, s);
        n_vg.p_hivpop_virgin(va0, s) *= 1.0 + i_dp.migration_rate(a0, s);

        for (int hm = 0; hm < hDS; ++hm) {
          n_vg.h_hivpop_virgin(hm, va0, s) *= 1.0 + i_dp.migration_rate(a0, s);
          if (t > opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              n_vg.h_artpop_virgin(hu, hm, va0, s) *= 1.0 + i_dp.migration_rate(a0, s);
            }
          }
        }
      }

      // Becoming sexually active
      for (int va = 0; va < SS::vAG; ++va) {
        n_vg.p_totpop_virgin(va, s) *= 1.0 - p_vg.sexdebut_annual_prob(va, s, t);
        n_vg.p_hivpop_virgin(va, s) *= 1.0 - p_vg.sexdebut_annual_prob(va, s, t);

        for (int hm = 0; hm < hDS; ++hm) {
          n_vg.h_hivpop_virgin(hm, va, s) *= 1.0 - p_vg.sexdebut_annual_prob(va, s, t);

          if (t > opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              n_vg.h_artpop_virgin(hu, hm, va, s) *= 1.0 - p_vg.sexdebut_annual_prob(va, s, t);
            }
          }
        }
      }
    } // s
  };

  // private methods that we don't want people to call
  private:
  // Returns survival probability for HIV+ people at age a, adjusted down for PWID excess
  // mortality. Only applies to working ages 15-49; returns surv unchanged outside that range.
  //
  // This is not used when running with direct incidence input
  // (incidence_model_choice == 0) or if pwid_sex_ratio < 0. Spectrum uses
  // pwid_sex_ratio = -1 to indicate that PWID adjustment is not used.
  //
  // pwid_hivpos_nonaids_mortality is expected as a rate (0-1)
  real_type pwid_adjusted_surv(real_type surv, int a, int s) {
    const bool no_adjustment = pars.ha.incidence_model_choice == SS::INCIDMOD_DIRECTINCID_HTS ||
      a < p_idx_hiv_first_adult || a >= 50 ||
      pars.ha.pwid_sex_ratio(t) < 0.0;

    if (no_adjustment) return surv;

    const real_type sex_ratio = pars.ha.pwid_sex_ratio(t);
    const real_type pop_s     = state_curr.dp.p_totpop(a - 1, s);
    const real_type total_pop = state_curr.dp.p_totpop(a - 1, SS::MALE) + state_curr.dp.p_totpop(a - 1, SS::FEMALE);
    if (total_pop <= 0.0 || pop_s <= 0.0) return surv;

    const real_type ratio = (s == SS::MALE)
      ? 1.0 / (1.0 + sex_ratio)
      : sex_ratio / (1.0 + sex_ratio);
    const real_type background_mort = 1.0 - surv;
    const real_type raw_excess = pars.ha.pwid_hivpos_nonaids_mortality - background_mort;
    const real_type excess = (raw_excess > 0.0 ? raw_excess : 0.0)
      * pars.ha.pwid_prop_hivpop(t) * ratio / (pop_s / total_pop);
    return 1.0 - (background_mort + excess);
  };

  void run_hiv_ageing_and_mortality() {
    const auto& p_dp = pars.dp;
    const auto& c_ha = state_curr.ha;
    auto& n_ha = state_next.ha;

    for (int s = 0; s < NS; ++s) {
      for (int a = 1; a < pAG; ++a) {
        const real_type surv = p_dp.survival_probability(a, s, t);
        n_ha.p_deaths_background_hivpop(a, s) = c_ha.p_hivpop(a - 1, s) * (1.0 - pwid_adjusted_surv(surv, a, s));
        n_ha.p_hivpop(a, s) = c_ha.p_hivpop(a - 1, s);
      }

      // open age group
      n_ha.p_deaths_background_hivpop(pAG - 1, s) +=
        c_ha.p_hivpop(pAG - 1, s) * (1 - p_dp.survival_probability(pAG, s, t));
      n_ha.p_hivpop(pAG - 1, s) += c_ha.p_hivpop(pAG - 1, s);
    }
  };

  void run_age_15_entrants() {
    static_assert(ModelVariant::run_child_model,
                  "run_age_15_entrants can only be called for model variants where run_child_model is true");
    constexpr int hcTT = SS::hcTT;
    constexpr int hc2AG = SS::hc2AG;
    constexpr int hc2DS = SS::hc2DS;

    const auto& p_hc = pars.hc;
    const auto& c_hc = state_curr.hc;
    auto& i_hc = intermediate.hc;

    for (int s = 0; s < NS; ++s) {
      for (int hm = 0; hm < hDS; ++hm) {
        i_hc.age15_hivpop(hm, s) = 0.0;
        for (int hm_adol = 0; hm_adol < hc2DS; ++hm_adol) {
          auto age15_hivpop_hm_adol = 0.0;
          for (int htm = 0; htm < hcTT; ++htm) {
            age15_hivpop_hm_adol += c_hc.hc2_hivpop(hm_adol, htm, (hc2AG - 1), s);
          }
          i_hc.age15_hivpop(hm, s) += age15_hivpop_hm_adol * SS::hc2_to_ha_cd4_transition[hm][hm_adol];
        }

        if (t > p_hc.hc_art_start) {
          // Children on ART enter the adult model in the longest ART duration, whatever their child duration
          // (Spectrum 6.43, ChAged14ByCD4Cat against age-15 deaths and ART; adult first-year ART mortality otherwise
          // applies to children already on treatment)
          for (int hu = 0; hu < hTS; ++hu) i_hc.age15_artpop(hu, hm, s) = 0.0;
          for (int hu = 0; hu < hTS; ++hu) {
            for (int hm_adol = 0; hm_adol < hc2DS; ++hm_adol) {
              i_hc.age15_artpop(hTS - 1, hm, s) += c_hc.hc2_artpop(hu, hm_adol, (hc2AG - 1), s)  * SS::hc2_to_ha_cd4_transition[hm][hm_adol];
            }
          }
        }
      } // hm
    } // s
  };

  void run_hiv_and_art_stratified_ageing() {
    const auto& c_ha = state_curr.ha;
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    // age coarse stratified HIV population
    for (int s = 0; s < NS; ++s) {
      int a = p_idx_hiv_first_adult;
      // Note: loop stops at hAG-1; no one ages out of the open-ended
      // age group
      for (int ha = 0; ha < (hAG - 1); ++ha) {
        for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
          i_ha.hiv_age_up_prob(ha, s) += c_ha.p_hivpop(a, s);
        }

        if (i_ha.hiv_age_up_prob(ha, s) > 0) {
          i_ha.hiv_age_up_prob(ha, s) = c_ha.p_hivpop(a - 1, s) / i_ha.hiv_age_up_prob(ha, s);
        } else {
          i_ha.hiv_age_up_prob(ha, s) = 0.0;
        }
      }
    }

    for (int s = 0; s < NS; ++s) {
      for (int ha = 1; ha < hAG; ++ha) {
        for (int hm = 0; hm < hDS; ++hm) {
          n_ha.h_hivpop(hm, ha, s) = (1.0 - i_ha.hiv_age_up_prob(ha, s)) * c_ha.h_hivpop(hm, ha, s);  // age-out
          n_ha.h_hivpop(hm, ha, s) += i_ha.hiv_age_up_prob(ha - 1, s) * c_ha.h_hivpop(hm, ha - 1, s); // age-in

          if (t > opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              n_ha.h_artpop(hu, hm, ha, s) = (1.0 - i_ha.hiv_age_up_prob(ha, s)) * c_ha.h_artpop(hu, hm, ha, s);  // age-out
              n_ha.h_artpop(hu, hm, ha, s) += i_ha.hiv_age_up_prob(ha - 1, s) * c_ha.h_artpop(hu, hm, ha - 1, s); // age-in
            }
          }
        }
      }

      // age out of ha = 0 group
      int ha = 0;
      for (int hm = 0; hm < hDS; ++hm) {
        n_ha.h_hivpop(hm, ha, s) = (1.0 - i_ha.hiv_age_up_prob(ha, s)) * c_ha.h_hivpop(hm, ha, s);  // age-out
        if (t > opts.ts_art_start) {
          for (int hu = 0; hu < hTS; ++hu) {
            n_ha.h_artpop(hu, hm, ha, s) = (1.0 - i_ha.hiv_age_up_prob(ha, s)) * c_ha.h_artpop(hu, hm, ha, s);  // age-out
          }
        }
      }

      // Entrants ageing into adult HIV population
      if constexpr (ModelVariant::run_child_model) {
        auto& i_hc = intermediate.hc;
        const auto& p_hc = pars.hc;

        for (int hm = 0; hm < hDS; ++hm) {
          n_ha.h_hivpop(hm, 0, s) += i_hc.age15_hivpop(hm, s);

          if (t > p_hc.hc_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              if (t > opts.ts_art_start) {
                n_ha.h_artpop(hu, hm, 0, s) += i_hc.age15_artpop(hu, hm, s);
              } else {
                // If child ART has started, but not yet adult ART has started, put
                // children on ART into the untreated adult HIV population
                n_ha.h_hivpop(hm, 0, s) += i_hc.age15_artpop(hu, hm, s);
              }
            }
          }
        }
      }
    } // s
  };

  // Exogenous age-15 entrants for variants that do not simulate paediatric
  // dynamics. Mirrors Spectrum/EPP-ASM, which reads the age-15 HIV+ inflow and
  // its CD4 composition from a prior paediatric projection rather than
  // simulating it (eppasm/R/eppasm.R, entrantprev/entrantartcov/paedsurv_*).
  //
  // Runs after run_hiv_and_art_stratified_ageing(), which *assigns* the ha = 0
  // compartments; entrants are added on top, exactly as the child-model path
  // does. p_hivpop is updated in step because the single-age and CD4-stratified
  // adult populations are required to agree.
  void run_static_age_15_entrants() {
    static_assert(!ModelVariant::run_child_model,
                  "run_static_age_15_entrants would double-count entrants when "
                  "the child model is simulated");
    const auto& p_en = pars.en;
    auto& n_dp = state_next.dp;
    auto& n_ha = state_next.ha;
    constexpr int a0 = p_idx_hiv_first_adult;

    for (int s = 0; s < NS; ++s) {
      // The age-15 population comes from the demographic projection, which has
      // already run for this year without paediatric HIV deaths; only its HIV+
      // share is supplied here.
      const real_type pop15 = n_dp.p_totpop(a0, s);
      const real_type prev15 = p_en.entrant_hiv_prevalence(s, t);
      const real_type hivp_entrants = pop15 * prev15;

      // HIV-negative entrants as eppasm (simmod, no popadjust) and EPP's age
      // model (AgeModel.run_hiv_model) build them:
      //   birthslag * cumsurv * (1 - entrantprev / paedsurv_lag)
      //     + cumnetmigr * (1 - pregprevlag * netmig_hivprob)
      // Children who died of HIV before 15 are missing from the surviving
      // births: of those reaching 15 HIV+ with probability prev15, a share
      // prev15 / paedsurv was infected. The cohort's net migrants are HIV+ with
      // probability netmig_hivprob times births-weighted prevalence among
      // pregnant women 15 years earlier (pregprev_lag, filled by
      // run_static_entrants_pregprev). Both inputs are the previous year's
      // values (eppasm: paedsurv_lag[i - 1], cumnetmigr[, i - 1]). With no net
      // migration and paedsurv = 1 the cohort is left as is.
      const real_type paedsurv = p_en.entrant_paediatric_survival(t - 1);
      const real_type migrants = p_en.entrant_net_migration(s, t - 1);
      const real_type pregprev_lag = state_curr.en.pregprev_lag(p_idx_hiv_first_adult - 1);
      if (paedsurv > 0.0) {
        n_dp.p_totpop(a0, s) = (pop15 - migrants) * (1.0 - prev15 / paedsurv) +
          migrants * (1.0 - pregprev_lag * p_en.entrant_net_migration_hiv_prob) +
          hivp_entrants;
      }
      if (!(hivp_entrants > 0.0)) {
        continue;
      }

      const bool art_started = t > opts.ts_art_start;
      const real_type artcov =
        art_started ? p_en.entrant_art_coverage(s, t) : 0.0;
      const real_type entrants_no_art = hivp_entrants * (1.0 - artcov);
      const real_type entrants_art = hivp_entrants * artcov;

      n_ha.p_hivpop(a0, s) += hivp_entrants;

      for (int hm = 0; hm < hDS; ++hm) {
        n_ha.h_hivpop(hm, 0, s) +=
          entrants_no_art * p_en.entrant_cd4_distribution(hm, s, t);

        if (entrants_art > 0.0) {
          for (int hu = 0; hu < hTS; ++hu) {
            n_ha.h_artpop(hu, hm, 0, s) +=
              entrants_art * p_en.entrant_art_cd4_distribution(hu, hm, s, t);
          }
        }
      }
    } // s
  };

  // Births by fertile HIV age group in EPP's age model: the female population
  // averaged over the start and end of the year's demographic projection
  // (after this year's age-15 entrants, before HIV deaths) times
  // age-specific fertility (AgeModel.run_hiv_model, births_by_ha).
  void run_static_entrants_births_by_fertility_age() {
    const auto& p_dp = pars.dp;
    const auto& c_dp = state_curr.dp;
    const auto& n_dp = state_next.dp;
    auto& i_en = intermediate.en;

    int a = p_idx_fertility_first;
    for (int ha = 0; ha < hAG_fertility; ++ha) {
      i_en.births_by_fertility_age(ha) = 0.0;
      for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
        i_en.births_by_fertility_age(ha) += (c_dp.p_totpop(a, FEMALE) + n_dp.p_totpop(a, FEMALE)) / 2.0 *
          p_dp.age_specific_fertility_rate(a - p_idx_fertility_first, t);
      }
    }
  };

  void run_hiv_and_art_stratified_deaths_and_migration() {
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;
    auto& i_dp = intermediate.dp;

    for (int s = 0; s < NS; ++s) {
      int a = p_idx_hiv_first_adult;
      for (int ha = 0; ha < hAG; ++ha) {
        for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
          i_ha.p_coarse_ages_hivpop(ha, s) += n_ha.p_hivpop(a, s);
        }
      }
    }

    // remove non-HIV deaths and net migration from hiv positive single-age population
    for (int s = 0; s < NS; ++s) {
      for (int a = 1; a < pAG; ++a) {
        n_ha.p_hivpop(a, s) -= n_ha.p_deaths_background_hivpop(a, s);
        if (opts.proj_period_int == PROJPERIOD_MIDYEAR) {
          n_ha.p_net_migration_hivpop(a, s) = n_ha.p_hivpop(a, s) * i_dp.migration_rate(a, s);
          n_ha.p_hivpop(a, s) += n_ha.p_net_migration_hivpop(a, s);
        }
      }
    }

    // remove non-HIV deaths and net migration from adult HIV stage stratified population
    for (int s = 0; s < NS; ++s) {
      int a = p_idx_hiv_first_adult;
      for (int ha = 0; ha < hAG; ++ha) {
        real_type deaths_migrate = 0.0;
        for (int i = 0; i < hAG_span[ha]; ++i, ++a) {
          deaths_migrate -= n_ha.p_deaths_background_hivpop(a, s);
          if (opts.proj_period_int == PROJPERIOD_MIDYEAR) {
            deaths_migrate += n_ha.p_net_migration_hivpop(a, s);
          }
        }

        real_type deaths_migrate_rate = 0.0;
        if (i_ha.p_coarse_ages_hivpop(ha, s) > 0) {
          deaths_migrate_rate = deaths_migrate / i_ha.p_coarse_ages_hivpop(ha, s);
        }

        for (int hm = 0; hm < hDS; ++hm) {
          n_ha.h_hivpop(hm, ha, s) *= 1.0 + deaths_migrate_rate;
          if (t > opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) {
              n_ha.h_artpop(hu, hm, ha, s) *= 1.0 + deaths_migrate_rate;
            }
          }
        }
      }
    }
  };

  void run_hc_hiv_and_art_stratified_deaths_and_migration() {
    static constexpr int hc2_agestart = SS::hc2_agestart;
    static constexpr int hcAG_end = SS::hcAG_end;
    static constexpr int hc1DS = SS::hc1DS;
    static constexpr int hc2DS = SS::hc2DS;
    static constexpr int hTS = SS::hTS;
    static constexpr int hcTT = SS::hcTT;

    auto& n_ha = state_next.ha;
    auto& n_hc = state_next.hc;
    const auto& p_hc = pars.hc;

    for (int s = 0; s < NS; ++s) {
      for (int a = 0; a < hcAG_end; ++a) {
        real_type deaths_migrate = 0.0;
        deaths_migrate -= n_ha.p_deaths_background_hivpop(a, s);
        if (opts.proj_period_int == PROJPERIOD_MIDYEAR) {
          deaths_migrate += n_ha.p_net_migration_hivpop(a, s);
        }
        if (n_ha.p_hivpop(a, s) > 0) {
          deaths_migrate /= n_ha.p_hivpop(a, s);
        }
        if (a < hc2_agestart) {
          for (int cat = 0; cat < hcTT; ++cat) {
            for (int hd = 0; hd < hc1DS; ++hd) {
              n_hc.hc1_hivpop(hd, cat, a, s) *= 1.0 + deaths_migrate;
            }
          }
          if (t > p_hc.hc_art_start) {
            for (int hd = 0; hd < hc1DS; ++hd) {
              for (int dur = 0; dur < hTS; ++dur) {
                n_hc.hc1_artpop(dur, hd, a, s) *= 1.0 + deaths_migrate;
              }
            }
          }
          } else {
            for (int hd = 0; hd < hc2DS; ++hd) {
              for (int cat = 0; cat < hcTT; ++cat) {
                n_hc.hc2_hivpop(hd, cat, a - hc2_agestart, s) *= 1.0 + deaths_migrate;
              }
              if (t > p_hc.hc_art_start) {
                for (int dur = 0; dur < hTS; ++dur) {
                  n_hc.hc2_artpop(dur, hd, a - hc2_agestart, s) *= 1.0 + deaths_migrate;
                }
              }
            }
          }

      }
    }
  };

};
}
}
