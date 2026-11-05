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

    i_ha.everARTelig_idx = p_ha.idx_hm_elig(t) < hDS ? p_ha.idx_hm_elig(t) : hDS;
    i_ha.anyelig_idx = p_ha.idx_hm_elig(t);

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

    } else if constexpr (ModelVariant::run_sr) {

      // Structural-rate incidence engine: SHIPP sexual-risk classes +
      // reduced-form KP overlay, interventions embedded in the FOI (§4).
      calc_new_infections_incidmod_sr(hiv_step);

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

  // Structural-rate incidence engine (methods §4; R reference
  // calc_infections_draft.R). Computes annualized new infections
  // i_ha.p_infections_ts(a, s) from a calibrated structural rate (reusing the
  // transmission_rate_hts slot as r_struct), SHIPP sexual-risk classes with
  // per-exposure intervention multipliers M, and a reduced-form key-population
  // overlay. Only p_infections_ts is written here; the shared CD4/ART cascade
  // downstream (add_new_hiv_infections, progression, mortality) stays
  // class-agnostic (partial stratification, §9). Gated by run_sr.
  void calc_new_infections_incidmod_sr(int hiv_step) {
    const auto& p_ha = pars.ha;
    const auto& p_sr = pars.sr;
    auto& n_ha = state_next.ha;
    auto& n_dp = state_next.dp;
    auto& n_sr = state_next.sr;
    auto& i_ha = intermediate.ha;

    constexpr int NCLASS = SS::NCLASS;
    constexpr int NKP = SS::NKP;
    const int a_lo = SS::pIDX_15to49;
    const int a_hi = SS::pIDX_15to49 + SS::pAG_15to49;          // 15-49 pool
    const int current_hiv_time_step = t * opts.hts_per_year + hiv_step;
    const real_type r_struct = p_ha.transmission_rate_hts[current_hiv_time_step];

    // -- 1. Infectiousness weight omega_s (ART-as-prevention; §4.2). off-ART
    //    infectiousness = 1 (matches base transmrate kernel — per-CD4 psi is a
    //    future refinement); on-ART = relative_infectiousness_art. -----------
    real_type omega[NS];
    for (int s = 0; s < NS; ++s) {
      real_type noart = 0.0, onart = 0.0;
      for (int ha = SS::hIDX_15to49; ha < SS::hIDX_15to49 + SS::hAG_15to49; ++ha) {
        for (int hm = 0; hm < hDS; ++hm) {
          noart += n_ha.h_hivpop(hm, ha, s);
          if (t >= opts.ts_art_start) {
            for (int hu = 0; hu < hTS; ++hu) onart += n_ha.h_artpop(hu, hm, ha, s);
          }
        }
      }
      const real_type npos = noart + onart;
      omega[s] = npos > 0.0 ? (noart + p_ha.relative_infectiousness_art * onart) / npos : 0.0;
    }

    // -- 1b. At-risk HIV-negative population by age/sex. Under run_virgin the
    //    pre-debut ("virgin") HIV-negatives are removed from the sexual at-risk
    //    pool (this closes the transmission-rate virgin TODO): pre-debut cannot
    //    acquire sexual infection. Sexual debut (sexdebut_annual_prob) moves
    //    them out of the virgin compartment into the at-risk SHIPP classes,
    //    which is what produces age-appropriate 10-14 incidence.
    real_type atrisk_hivn[pAG][NS];
    for (int s = 0; s < NS; ++s)
      for (int a = 0; a < pAG; ++a)
        atrisk_hivn[a][s] = std::max(real_type(0.0), n_dp.p_totpop(a, s) - n_ha.p_hivpop(a, s));
    if constexpr (ModelVariant::run_virgin) {
      const auto& n_vg = state_next.vg;
      for (int s = 0; s < NS; ++s)
        for (int va = 0, a = SS::p_idx_virginpop_first; va < SS::vAG; ++va, ++a)
          atrisk_hivn[a][s] = std::max(real_type(0.0),
            atrisk_hivn[a][s] - (n_vg.p_totpop_virgin(va, s) - n_vg.p_hivpop_virgin(va, s)));
    }

    // -- 2. Rescale the light per-class infected headcount Ig to the current
    //    HIV+ 15-49 count (class shares preserved). Keeps Ig consistent with
    //    true prevalence while class structure evolves via new infections
    //    (the virgin module's proportional-cascade idiom). NOTE: rescales to
    //    total HIV+ incl. KP infecteds — a documented simplification (folds KP
    //    into the pool mean-field; refine by tracking gen-pop prevalence). ---
    //    Redistribute the true HIV+ headcount across classes by the current
    //    shares (bounded in [0,1]); this keeps Ig total == prevalence exactly
    //    and bounded (setting Ig = share*hivpos avoids the divide-by-tiny
    //    blow-up an unbounded multiplicative rescale would cause as the class
    //    headcount collapses). Fall back to contact-weighted shares when the
    //    headcount has effectively vanished.
    //    sr_class_memory 1 carries the class headcount over from the end of
    //    the previous year; 0 (default) starts each year from the zeroed state,
    //    i.e. from the fallback below weighted by contact rate alone, ignoring
    //    class sizes. State is reset every year, so without the carry the class
    //    composition of the infected only evolves within a year, and splitting
    //    a class changes the pool even at equal contact rates. Use mode 1 with
    //    the non-regular partner-count split.
    if (p_sr.sr_class_memory == 1 && hiv_step == 0) {
      for (int s = 0; s < NS; ++s)
        for (int g = 0; g < NCLASS; ++g)
          n_sr.sr_infected_class(s, g) = state_curr.sr.sr_infected_class(s, g);
    }
    constexpr real_type kMinIg = 1e-8;
    for (int s = 0; s < NS; ++s) {
      real_type hivpos_s = 0.0;
      for (int a = a_lo; a < a_hi; ++a) hivpos_s += std::max(real_type(0.0), n_ha.p_hivpop(a, s));
      real_type sumIg = 0.0;
      for (int g = 0; g < NCLASS; ++g) sumIg += n_sr.sr_infected_class(s, g);
      if (sumIg > kMinIg) {
        for (int g = 0; g < NCLASS; ++g)
          n_sr.sr_infected_class(s, g) = (n_sr.sr_infected_class(s, g) / sumIg) * hivpos_s;
      } else {
        // Fallback shares: contact rate alone (mode 0), or contact rate x
        // class size among susceptibles (mode 1), the composition new
        // infections take, so splitting a class into sub-classes of equal
        // contact rate leaves the pool unchanged.
        real_type w[NCLASS];
        for (int g = 0; g < NCLASS; ++g) {
          w[g] = p_sr.sr_contact_rate(g);
          if (p_sr.sr_class_memory == 1) {
            real_type Sg = 0.0;
            for (int a = a_lo; a < a_hi; ++a) {
              real_type qtot = 0.0;
              for (int k = 0; k < NKP; ++k) qtot += p_sr.sr_kp_prop(a, s, k, t);
              if (qtot > 1.0) qtot = 1.0;
              Sg += p_sr.sr_class_prop(a, s, g, t) * (1.0 - qtot) * atrisk_hivn[a][s];
            }
            w[g] *= Sg;
          }
        }
        real_type csum = 0.0;
        for (int g = 0; g < NCLASS; ++g) csum += w[g];
        for (int g = 0; g < NCLASS; ++g)
          n_sr.sr_infected_class(s, g) = csum > 0.0
            ? (w[g] / csum) * hivpos_s
            : hivpos_s / NCLASS;
      }
    }

    // -- 3. Contact-weighted infectious prevalence pool P_eff (§4.1).
    //    Sg(a,s,g) = pi * (1 - sum_k q_k) * hivn.
    //    sr_mixing_mode 0 (default) pools both sexes into one scalar that every
    //    susceptible sees, so sex differences in exposure enter only through
    //    incidence_rate_ratio_sex. Mode 1 is the methods-appendix form: a
    //    susceptible of sex s draws partners from the opposite sex s',
    //    P_eff[s] = sum_g c_g omega_{s'} I_{s',g} / sum_g c_g N_{s',g}. --------
    static_assert(NS == 2, "opposite-sex mixing assumes two sexes");
    const bool opposite_sex = (p_sr.sr_mixing_mode == 1);
    real_type pool_num = 0.0, pool_den = 0.0;         // both sexes, mode 0
    real_type pool_num_s[NS] = {0.0, 0.0};            // by contributing sex
    real_type pool_den_s[NS] = {0.0, 0.0};
    for (int s = 0; s < NS; ++s) {
      for (int g = 0; g < NCLASS; ++g) {
        const real_type cg = p_sr.sr_contact_rate(g);
        real_type Nsg = n_sr.sr_infected_class(s, g);
        for (int a = a_lo; a < a_hi; ++a) {
          const real_type hivn = atrisk_hivn[a][s];
          real_type qtot = 0.0;
          for (int k = 0; k < NKP; ++k) qtot += p_sr.sr_kp_prop(a, s, k, t);
          if (qtot > 1.0) qtot = 1.0;
          Nsg += p_sr.sr_class_prop(a, s, g, t) * (1.0 - qtot) * hivn;
        }
        const real_type num_sg = cg * omega[s] * n_sr.sr_infected_class(s, g);
        const real_type den_sg = cg * Nsg;
        pool_num += num_sg;                            // same order as before
        pool_den += den_sg;                            // the option: mode 0
        pool_num_s[s] += num_sg;                       // is bit-identical
        pool_den_s[s] += den_sg;
      }
    }
    real_type P_eff[NS];
    for (int s = 0; s < NS; ++s) {
      if (opposite_sex) {
        const int so = 1 - s;
        P_eff[s] = pool_den_s[so] > 0.0 ? pool_num_s[so] / pool_den_s[so] : 0.0;
      } else {
        P_eff[s] = pool_den > 0.0 ? pool_num / pool_den : 0.0;
      }
    }

    // -- 4. Mean-preserving age/sex shape zeta, normalized over susceptibles
    //    (15-49) so it sets the incidence distribution, not its level (§4.1).
    //    Under mode 1 the weights also carry each sex's pool, so the
    //    susceptible-weighted 15-49 incidence is r_struct * Pbar, Pbar the
    //    susceptible-weighted mean of P_eff[s] (§4.1). zbar is one scalar for
    //    both sexes, so this moves only the level, never the sex ratio. -----
    real_type zbar_num = 0.0, zbar_den = 0.0;
    for (int s = 0; s < NS; ++s) {
      const real_type sexrr = (s == MALE) ? 1.0 : p_ha.incidence_rate_ratio_sex(t);
      const real_type wP = opposite_sex ? P_eff[s] : 1.0;
      for (int a = a_lo; a < a_hi; ++a) {
        const real_type hivn = atrisk_hivn[a][s];
        const real_type zeta = p_ha.incidence_rate_ratio_age(a - SS::p_idx_hiv_first_adult, s, t) * sexrr;
        if (opposite_sex) {
          zbar_num += zeta * wP * hivn;
          zbar_den += wP * hivn;
        } else {
          zbar_num += zeta * hivn;
          zbar_den += hivn;
        }
      }
    }
    const real_type zbar = zbar_den > 0.0 ? zbar_num / zbar_den : 1.0;

    // -- 5. General-pop FOI + KP overlay -> annualized infections by age/sex --
    const bool seeding = (p_ha.epidemic_start_hts == current_hiv_time_step);
    nda::fill(i_ha.p_infections_ts, 0.0);
    real_type newinf_class[NS][NCLASS];
    for (int s = 0; s < NS; ++s)
      for (int g = 0; g < NCLASS; ++g) newinf_class[s][g] = 0.0;

    for (int s = 0; s < NS; ++s) {
      const real_type sexrr = (s == MALE) ? 1.0 : p_ha.incidence_rate_ratio_sex(t);
      for (int a = SS::p_idx_hiv_first_adult; a < pAG; ++a) {
        const real_type hivn = atrisk_hivn[a][s];
        real_type qtot = 0.0;
        for (int k = 0; k < NKP; ++k) qtot += p_sr.sr_kp_prop(a, s, k, t);
        if (qtot > 1.0) qtot = 1.0;
        const real_type zeta = p_ha.incidence_rate_ratio_age(a - SS::p_idx_hiv_first_adult, s, t) * sexrr / zbar;

        real_type inf_gen_as = 0.0, Sg_as = 0.0;
        for (int g = 0; g < NCLASS; ++g) {
          const real_type cg = p_sr.sr_contact_rate(g);
          const real_type Sg = p_sr.sr_class_prop(a, s, g, t) * (1.0 - qtot) * hivn;
          const real_type lambda = r_struct * cg * P_eff[s] * zeta * p_sr.sr_intervention_mult(a, s, g, t);
          real_type inf = lambda * Sg;
          if (seeding) inf += p_ha.initial_incidence * Sg;     // epidemic seed
          i_ha.p_infections_ts(a, s) += inf;
          n_sr.sr_infections_class(s, g) += opts.dt * inf;
          inf_gen_as += inf;
          Sg_as += Sg;
          if (a >= a_lo && a < a_hi) newinf_class[s][g] += inf;
        }

        // KP overlay: lambda_k = irr_k * lam_bar_{a,s} * M_k on KP susceptibles
        const real_type lam_bar = Sg_as > 0.0 ? inf_gen_as / Sg_as : 0.0;
        for (int k = 0; k < NKP; ++k) {
          const real_type Skp = p_sr.sr_kp_prop(a, s, k, t) * hivn;
          const real_type inf_kp = p_sr.sr_kp_irr(k, t) * lam_bar *
                                   p_sr.sr_kp_intervention_mult(a, s, k, t) * Skp;
          i_ha.p_infections_ts(a, s) += inf_kp;
          n_sr.sr_infections_kp(s, k) += opts.dt * inf_kp;
        }
      }
    }

    // -- 6. Accumulate this step's new infections into the class headcount
    //    (annualized * dt); rescale to prevalence happens at the next step. --
    for (int s = 0; s < NS; ++s)
      for (int g = 0; g < NCLASS; ++g)
        n_sr.sr_infected_class(s, g) += opts.dt * newinf_class[s][g];
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
      allocate_art_initiation_by_age(s, hiv_step);
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

    for (int ha = hIDX_15PLUS; ha < hAG; ++ha) {
      for (int hm = i_ha.everARTelig_idx; hm < hDS; ++hm) {
        if (hm >= i_ha.anyelig_idx) {
          // TODO: Implement special population ART eligibility
          const real_type prop_elig = 1.0;
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
  void allocate_art_initiation_by_cd4() {
    const auto& p_ha = pars.ha;
    auto& i_ha = intermediate.ha;

    for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) {
      real_type eligibility_by_stage = 0.0;
      if (i_ha.Xartelig_15plus > 0.0) {
        eligibility_by_stage = (1.0 - p_ha.initiation_mortality_weight) *
                               i_ha.artelig_hm(hm) /
                               i_ha.Xartelig_15plus;
      }

      real_type expected_mortality_by_stage = 0.0;
      if (i_ha.expect_mort_artelig15plus > 0.0) {
        expected_mortality_by_stage = p_ha.initiation_mortality_weight *
                                      i_ha.expect_mort_artelig_hm(hm) /
                                      i_ha.expect_mort_artelig15plus;
      }

      i_ha.artinit_hm(hm) = i_ha.artinit_hts * (eligibility_by_stage + expected_mortality_by_stage);
    }
  };

  // Step 2: within CD4 stage, allocate ART initiations by age proportional to
  // eligibility, capped by the population actually available to initiate
  void allocate_art_initiation_by_age(int s, int hiv_step) {
    auto& n_ha = state_next.ha;
    auto& i_ha = intermediate.ha;

    // ART initiation replay (HivStructuralRate only). sr_art_init_mode 0
    // derives initiations from the ART target exactly as below and records
    // them per HIV step, as counts (sr_art_init_hts) and as rates of the
    // available untreated pool (sr_art_init_rate_hts). Mode 1 ignores the
    // target and replays sr_art_init_replay as COUNTS; mode 2 replays it as
    // RATES of the pool, so initiations scale with the scenario's own need
    // (coverage space, for decomposition counterfactuals). Either way
    // initiation no longer back-fills a target, so it can be switched
    // independently of ART dropout (maintenance). Replaying a mode-0 record
    // reproduces that run (mode 1 exactly, mode 2 to rounding).
    if constexpr (ModelVariant::run_sr) {
      for (int ha = 0; ha < hAG; ++ha)
        for (int hm = 0; hm < hDS; ++hm) {
          state_next.sr.sr_art_init_hts(hm, ha, s, hiv_step) = 0.0;
          state_next.sr.sr_art_init_rate_hts(hm, ha, s, hiv_step) = 0.0;
        }
    }

    for (int ha = hIDX_15PLUS; ha < hAG; ++ha) {
      for (int hm = i_ha.anyelig_idx; hm < hDS; ++hm) {
        real_type artinit_hahm = 0.0;
        bool replayed = false;
        const real_type pool = n_ha.h_hivpop(hm, ha, s) + opts.dt * i_ha.grad(hm, ha, s);
        if constexpr (ModelVariant::run_sr) {
          const int mode = pars.sr.sr_art_init_mode;
          if (mode == 1 || mode == 2) {
            const real_type x = pars.sr.sr_art_init_replay(hm, ha, s, hiv_step, t);
            artinit_hahm = std::max(std::min(mode == 1 ? x : x * pool, pool), 0.0);
            replayed = true;
          }
        }
        if (!replayed) {
          if (!(i_ha.artelig_hm(hm) > 0.0)) continue;
          artinit_hahm = i_ha.artinit_hm(hm) *
                         i_ha.artelig_hahm(hm, ha - hIDX_15PLUS) /
                         i_ha.artelig_hm(hm);
          artinit_hahm = std::min(artinit_hahm, i_ha.artelig_hahm(hm, ha - hIDX_15PLUS));
          artinit_hahm = std::min(artinit_hahm, pool);
        }
        i_ha.grad(hm, ha, s) -= artinit_hahm / opts.dt;
        i_ha.gradART(ART0MOS, hm, ha, s) += artinit_hahm / opts.dt;
        n_ha.h_art_initiation(hm, ha, s) += artinit_hahm;
        if constexpr (ModelVariant::run_sr) {
          state_next.sr.sr_art_init_hts(hm, ha, s, hiv_step) = artinit_hahm;
          state_next.sr.sr_art_init_rate_hts(hm, ha, s, hiv_step) =
            pool > 0.0 ? artinit_hahm / pool : 0.0;
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
