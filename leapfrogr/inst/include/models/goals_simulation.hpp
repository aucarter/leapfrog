#pragma once

#include <iostream>
#include <list>

#include "../generated/config_mixer.hpp"
#include "../options.hpp"

namespace leapfrog {
namespace internal {

template<typename Config>
concept GoalsSimulationEnabled = RunDemographicProjection<Config>
    && RunHivSimulation<Config> && RunChildModel<Config> && RunGoals<Config>;

template<typename Config>
struct GoalsSimulation {
  GoalsSimulation(...) {};
};

template<GoalsSimulationEnabled Config>
struct GoalsSimulation<Config> {
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
  static constexpr int nNS = SS::nNS;
  static constexpr int nRG = SS::nRG;
  static constexpr int nVAC = SS::nVAC;
  static constexpr int nHIV = SS::nHIV;
  static constexpr int nART = SS::nART;
  static constexpr int nCD4 = SS::nCD4;
  static constexpr int nCD4p = SS::nCD4p;
  static constexpr int nCD4a = SS::nCD4a;

  static constexpr int pAG = SS::pAG;
  static constexpr int pIDX_24to35 = SS::pIDX_24to35;
  static constexpr int pIDX_15to49 = SS::pIDX_15to49;
  static constexpr int pAges5Yrs = SS::pAges5Yrs;
  static constexpr int rRG_TOTAL = SS::rRG_TOTAL;
  static constexpr int rRG_MSM_F3 = SS::rRG_MSM_F3;
  static constexpr int rRG_DUR = SS::rRG_DUR;
  static constexpr int vVAC_NO_PROT = SS::vVAC_NO_PROT;
  static constexpr int vVAC_NUM_PARS = SS::vVAC_NUM_PARS;
  static constexpr int vVAC_NUM_EFFECTS = SS::vVAC_NUM_EFFECTS;
  static constexpr int hDS = SS::hDS;
  static constexpr int hTS = SS::hTS;
  static constexpr int hCD4_ALL_HIV = SS::hCD4_ALL_HIV;
  static constexpr int hCD4_LT50 = SS::hCD4_LT50;
  static constexpr int hHIV_Cats = SS::hHIV_Cats;

  static constexpr int hc2_agestart = SS::hc2_agestart;
  static constexpr int hcTT = SS::hcTT;
  static constexpr int hc1DS = SS::hc1DS;
  static constexpr int hc2DS = SS::hc2DS;

  enum Index {
    // Sexes
    S_MALE = 0,
    S_FEMALE = 1,
    S_ALL = 2,
    // S_OFFSET =1, //offset of 1 with respect to goals inputs

    // Risk Groups
    RG_NONE = 0,
    RG_LRH = 1,
    RG_MRH = 2,
    RG_HRH = 3,
    RG_IDU = 4,
    RG_MSM = 5,
    RG_TOTAL1 = 5,
    RG_TOTAL_ART = 6,
    RG_MSMLR = 6,
    RG_MSMMR = 7,
    RG_MSMHR = 8,
    RG_MSMIDU = 9,
    RG_TOTAL = 10,
    RG_NONE_F3 = 11,
    RG_LRH_F3 = 12,
    RG_MRH_F3 = 13,
    RG_HRH_F3 = 14,
    RG_IDU_F3 = 15,
    RG_MSM_F3 = 16,
    RG_ALL = 17,
    // RG_OFFSET = 1, //offset of 1 with respect to goals inputs

    // Duration
    PERC_POP = 0,
    DUR_AVG = 1,

    // CD4 10 year age bins
    CA_15_24 = 1,
    CA_25_34 = 2,
    CA_35_44 = 3,
    CA_45_54 = 4,

    CD4_NEG = 0,
    CD4_PRIM = 1,
    CD4_GT500 = 2,
    CD4_350_500 = 3,
    CD4_250_349 = 4,
    CD4_200_249 = 5,
    CD4_100_199 = 6,
    CD4_50_99 = 7,
    CD4_LT50 = 8,
    CD4_GT500_ART = 9,
    CD4_350_500_ART = 10,
    CD4_250_349_ART = 11,
    CD4_200_249_ART = 12,
    CD4_100_199_ART = 13,
    CD4_50_99_ART = 14,
    CD4_LT50_ART = 15,
    CD4_ALL = 16,

    ART_GT12m = 2,

    POP_H_HIVNeg = 0,
    POP_H_NoART = 1,
    POP_H_ARTlt6m = 2,
    POP_H_ART6to12m = 3,
    POP_H_ARTgt12m = 4,
    POP_H_OnART = 5,
    POP_H_ALL = 6,

    INF_PRIM = 1,
    INF_ASYMPT = 2,
    INF_SYMPT_NO_ART = 3,
    INF_SYMPT_ART = 4,

    VAC_UNV = 0,
    VAC_TAKE = 1,
    VAC_PARTIAL = 2,
    VAC_NO_PROT = 3,
    VAC_ALL = 4,

    VAC_EFF = 0,
    VAC_INF = 1,
    VAC_PROG = 2,
    VAC_DUR = 3,
    VAC_TYPE = 4,

    VAC_TAKEACTION = 0,
    VAC_DEGREEACTION = 1,

    VAC_COV_ALLRISK = 0,
    VAC_COV_SINGLE = 1,


    VAC_TARGET_HIV_ALL = 0,
    VAC_TARGET_HIV_NEG = 1,

    CURE_COV_ALLRISK = 0,
    CURE_COV_SINGLE = 1,

    VMM_COV_ALLRISK = 0,
    VMM_COV_SINGLE = 1,

    POC_CD4 = 0,
    POC_VL = 1,

    HV_SUCC = 0,
    HV_INF = 1,

    INTVN_MC15_49 = 26,
    INTVN_ALL = 43,

    PREP_OralDaily = 0,
    PREP_OralMonthly = 1,
    PREP_OralPlusCon = 2,
    PREP_Inject1Mo = 3,
    PREP_Inject2Mo = 4,
    PREP_Inject6Mo = 5,
    PREP_Ring = 6,
    PREP_bNABs = 7,
    PREP_Implant = 8,
    PREP_PEP = 9,
    PREP_NUM = 9,

    PREP_EFFECT = 0,
    PREP_ADH = 1,
    PREP_SUB = 2,
    PREP_DUR = 3,

    // Intervention mapping
    RN_COM_MOB = 1,
    RN_MASS_MEDIA = 2,
    RN_HTS = 3,
    RN_CONDOMS = 4,
    RN_PRIM_TEACH = 5,
    RN_COMP_SEX_EDUC = 6,
    RN_OUT_OF_SCHOOL = 7,
    RN_ECON_STRENGTH = 8,
    RN_WORK_PLACE_STI = 9,

    // Most At-Risk populations
    RN_FSW_OUTREACH = 10,
    RN_MSW = 11,
    RN_MSM_OUTREACH = 12,
    RN_MSM_LUB = 13,
    RN_IDU_HARM_RED = 14,
    RN_IDU_CandT = 15,
    RN_IDU_OUTREACH = 16,
    RN_IDU_NSEP = 17,
    RN_IDU_DRUG_SUB = 18,
    // Male circumcision
    RN_MC15_49 = 19,
    RN_NUM_VMMC = 20,
    RN_MC_INFANT = 21,
    // PrEP
    RN_PrEP_OralDaily = 22,
    RN_PrEP_OralMonthly = 23,
    RN_PrEP_OralPlusCon = 24,
    RN_PrEP_Inject1Mo = 25,
    RN_PrEP_Inject2Mo = 26,
    RN_PrEP_Inject6Mo = 27,
    RN_PrEP_Ring = 28,
    RN_PrEP_bNABs = 29,
    RN_PrEP_Implant = 30,
    RN_PrEP_PEP = 31,

    // Medical services for Coverage
    RN_MALE_STITx = 32,
    RN_FEMALE_STITx = 33,
    RN_BLOOD_SAFE = 34,
    RN_PEP = 35,
    RN_ADSYRYNGE = 36,
    RN_RED_INJECT = 37,
    RN_UNIV_PREC = 38,
    // Vaccines
    RN_VACCINES = 39,
    RN_CURE_Adults = 40,
    RN_CURE_NEO = 41,
    RN_VMM = 42,
    RN_AHD_TX = 43,
    RN_POC_CD4_INT = 44,
    RN_POC_VL_INT = 45,
    // PMTCT
    RN_HIV_TEST_ANC = 46,
    RN_HIV_WOMAN_COUNS = 47,
    RN_EXPOSED_INFANT_Dx = 48,
    // ART
    RN_ADULT_ON_ART = 49,
    RN_CHILD_ON_ART = 50,

    // new interventions in general pop section
    RN_CONDOM_SUPPLY = 51,
    RN_ANC_TESTING = 52,
    RN_MAX_INTERVN = 52,

    RN_DIRECT_COSTS = 53,
    RN_PROGRAM_COSTS = 54,
    RN_TOTAL_COSTS = 55,
    RN_MAX_COSTS = 55,

    RN_MAX_BEHAV_INTVN = RN_IDU_DRUG_SUB,

    // behaviour intervention impacts
    HV_IM_CONDOMS_HIGH = 1,
    HV_IM_CONDOMS_MED = 2,
    HV_IM_CONDOMS_LOW = 3,
    HV_IM_CONDOMS_MSM = 4,
    HV_IM_CONDOMS_IDU = 5,
    HV_IM_CONDOMS_MSM_HIGH = 6,
    HV_IM_CONDOMS_MSM_MED = 7,
    HV_IM_CONDOMS_MSM_LOW = 8,
    HV_IM_CONDOMS_MSM_IDU = 9,
    HV_IM_PARTNERS_HIGH = 10,
    HV_IM_PARTNERS_MED = 11,
    HV_IM_PARTNERS_LOW = 12,
    HV_IM_PARTNERS_MSM = 13,
    HV_IM_PARTNERS_IDU = 14,
    HV_IM_PARTNERS_MSM_HIGH = 15,
    HV_IM_PARTNERS_MSM_MED = 16,
    HV_IM_PARTNERS_MSM_LOW = 17,
    HV_IM_PARTNERS_MSM_IDU = 18,
    HV_IM_AGE_FIRST_SEX = 19,
    HV_IM_UNSAFE_INJECT_IDU = 20,
    HV_IM_NEEDLE_SHARING_IDU = 21,
    HV_IM_NEEDLE_SHARING_MSIDU = 22,
    HV_IM_NUM_SHARING_PARTNERS = 23,

    HV_MAX_BEHAV_IMPACTS = 23,

    RN_POP_CONDOM_WASTAGE = 8,
    RN_POP_SEC_SCHOOL_MALE = 12,
    RN_POP_SEC_SCHOOL_FEMALE = 13,
    RN_POP_Rapes = 17,
    RN_POP_IDU_OPIOD_DEP = 22,
    RN_POP_NUM_INJECT_YEAR = 23,
    RN_POP_PEPKitsRequired = 30,

    DP_EligTreatPregnantWomen = 1,
    DP_EligTreatTB_HIV = 2,
    DP_EligTreatDiscordantCouples = 3,
    DP_EligTreatSexWorkers = 4,
    DP_EligTreatMSM = 5,
    DP_EligTreatIDU = 6,
    DP_EligTreatOtherPop = 7,
    DP_EligTreatPopsMax = 7,
  };

  // function args
  int t;
  const Pars& pars;
  const State& state_curr;
  State& state_next;
  Intermediate& intermediate;
  const Options<real_type>& opts;

  // only exposing the constructor and some methods
public:
  GoalsSimulation(Args& args) :
      t(args.t),
      pars(args.pars),
      state_curr(args.state_curr),
      state_next(args.state_next),
      intermediate(args.intermediate),
      opts(args.opts) {};

  void run_goals_pre_hiv_loop() {
    const auto& p_hv = pars.hv;
    auto& n_hv = state_next.hv;
    const auto& c_hv = state_curr.hv;
    auto& i_hv = intermediate.hv;

    // initialize annual variables
    init_vars_pre_hiv_loop(t);

    // TODO: Turn this back on
    // Set coverage chnage for impact adj
    //if (t > p_hv.goals_base_year_idx) {
    //  calc_adj_matrix(t);
    //  calc_behav_matrix_impacts(t);
    //}

    // set risk group sizes from inputs
    // CDP move to t==1 when new variable type than intermediate is available
    set_riskgroup_proportions();

    // initilize adult population
    if (t == 1) {
      set_initial_pop();
    } else {
      // next state variables are cleared, make copy from current
      for (int v = VAC_UNV; v <= VAC_ALL; ++v) {
        for (int rg = RG_NONE; rg <= RG_ALL; ++rg) {
          for (int hd = CD4_NEG; hd <= CD4_ALL; ++hd) {
            for (int s = S_MALE; s <= S_ALL; ++s) {
              n_hv.adults(v, rg, hd, s) = c_hv.adults(v, rg, hd, s);
            }
          }
        }
      }

      // next state variables are cleared, make copy from current
      for (int rg = RG_NONE; rg <= RG_MSMIDU; ++rg) {
        for (int s = S_MALE; s <= S_FEMALE; ++s) {
          n_hv.cured_pop(rg, s) = c_hv.cured_pop(rg, s);
        }
      }

      // next state variables are cleared, make copy from current
      for (int s = S_MALE; s <= S_FEMALE; ++s) {
        n_hv.new_inf_scale_factor(s) = c_hv.new_inf_scale_factor(s);
      }
    }

    // set these rates to vars in goals
    if (t > p_hv.goals_base_year_idx) {
      calc_HIV_mort_adjustments(t);
    }

  }

  void run_goals_hiv_loop(int hiv_step) {
    const auto& p_hv = pars.hv;
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;

    // initialize inner loop varaibles
    init_vars_hiv_loop();

    // sum over the dimensions of adult pop
    sum_adult_pop_dims(t);

    // make inner-time loop copy of adult structure
    set_adults_hiv_loop();

    // collect info from dp/aim to set demographical and HIV rates
    set_goals_vars_from_dp(t, hiv_step);

    //migration rate for goals calculated seperately
    if(hiv_step==0){
      calc_migration_rate(t);
      //std::cout << "males, migration rate: t " << t  << " " << i_hv.migration_rate(0) << std::endl;
      //std::cout << "females, migration rate: t " << t  << " " << i_hv.migration_rate(1) << std::endl;
      //std::cout << "migration rates " << std::endl;
    }

    // set the rates to vars in goals, including progression, aging and mortality
    calc_goals_rates(t, hiv_step);

    // set vaccination vars
    calc_vac_effect(t);
    if ((hiv_step == 0) && (t > p_hv.goals_base_year_idx)) {
      calc_new_vaccinations(t);
    }

    // adjust distribution of new entrants into different risk groups
    if (t >= 2) {
      calc_newrecruits_distribution(t, hiv_step);
    }

    // calc multiplier and new infections
    bool has_plhiv = false;
    if (n_hv.adults(VAC_UNV, RG_HRH, CD4_350_500, S_FEMALE) > 0.0) {
      has_plhiv = true;
    }

    // progress, hiv-neg, not at risk (RG_NONE)
    progress_norisk_hiv_neg(t, hiv_step);

    // progress, hiv-pos and hiv-art, not at risk RG_NONE
    if (has_plhiv == true) {
      progress_hivp_and_art(t, RG_NONE, RG_NONE);
    }

    if (has_plhiv == true) {
      // calc prevalence used for new infections calc
      calc_prevalence(t, hiv_step);
      // force of infection multipliers
      calc_r_multiplier(t);
      // calc new infections
      calc_new_infections(t, hiv_step);
      // TODO: enable this function again
      // rescale new infections to the level of sex to match
      //calc_rescale_infections(t);
    }

    // progress, hiv-neg, at risk (RG_LRH..RG_MSM)
    progress_atrisk_hiv_neg(t, RG_LRH, RG_MSM);

    // progress, hiv-pos and hiv-art (RG_NONE..RG_MSM)
    if (has_plhiv == true) {
      progress_hivp_and_art(t, RG_LRH, RG_MSM);

      // ART allocation
      if (t >= opts.ts_art_start) {
        allocate_art(t);
      }
    }

    bool has_new_inf = false;
    if (n_hv.new_inf_vrs(VAC_UNV, RG_HRH, S_FEMALE) > 0.0) {
      has_new_inf = true;
    }

    // sum over the dimensions of adult pop
    sum_adult_pop_dims(t);

    // add new infections
    if (t >= ((p_hv.epi_start_year) - opts.proj_start_year)) {
      if (has_new_inf == true) {
        add_new_infections(t, hiv_step);
      }
      //add_new_infections_goals(t, hiv_step);
    }

    // at hiv first year, apply initial pulse, t starts at last hiv_step
    if ((t == ((p_hv.epi_start_year) - opts.proj_start_year))
        && (hiv_step == opts.hts_per_year - 1))
    {  // CDP reinstate correct time for pulse
      nda::fill(n_hv.new_infections_goals, 0.0); // incidence for DP, init at initial pulse year
      set_initial_pulse();
      if (n_hv.adults(VAC_UNV, RG_IDU, CD4_PRIM, S_MALE) == 0.0) {
        set_initial_pulse_idu();
      }
    }

   if( (t > p_hv.goals_base_year_idx) && (hiv_step == opts.hts_per_year - 1) ){
      calc_HIV_cure_coverage(t);
      calc_HIV_cure(t);
   }
  }

  void run_goals_post_hiv_loop() {
    const auto& p_hv = pars.hv;

    // sum over the dimensions of adult pop
    sum_adult_pop_dims(t);

    // set yearly goals outputs
    set_goals_outputs(t);

    if (t >= p_hv.goals_base_year_idx) {
      calc_resource_needs();
    }
  }

  void set_goals_outputs(int t) {
    /*
    Outputs to compare between leapfrog goals and spectrum delphi goals:

    n_hv.total_population: total population 15-49 both sexes

    n_hv.total_deaths: total deaths both sexes, all hiv states
    n_hv.total_deaths_hiv: total deaths both sexes, PLHIV

    n_hv.incidence_goals(S_ALL): incidence for both sexes [0,1]
    n_hv.new_infections_goals(S_ALL): new infections both sexes [0,1]

    n_hv.prevalence(RG_ALL,S_ALL): prevalence for both sexes [0,1]
    */

    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& c_hv = state_curr.hv;

    n_hv.total_population = n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_MALE)
        + n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_FEMALE);

    n_hv.total_plhiv = n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_MALE)
        + n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_FEMALE)
        - n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_MALE)
        - n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_FEMALE);

    real_type on_art = 0.0;
    for (int hd = CD4_GT500_ART; hd <= CD4_LT50_ART; ++hd) {
      on_art += n_hv.adults(VAC_ALL, RG_ALL, hd, S_ALL);
    }

    n_hv.total_on_art = on_art;

    n_hv.total_vaccinated = n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_ALL)-
                            n_hv.adults(VAC_UNV, RG_ALL, CD4_ALL, S_ALL);


    // toggle return here:
    return;

    real_type total_pop = n_hv.total_population;
    std::cout << "pop goals: t " << t << " " << total_pop << " " << std::endl;

    total_pop = i_hv.dp_pop_1549(S_MALE) + i_hv.dp_pop_1549(S_FEMALE);

    std::cout << "pop dp: t " << t << " " << total_pop << " " << std::endl;

    real_type deaths = n_hv.total_deaths;

    std::cout << "total deaths goals: t " << t << " " << deaths << " "
              << std::endl;

    real_type hiv_deaths = n_hv.total_deaths_hiv;

    std::cout << "total deaths hiv goals: t " << t << " " << hiv_deaths << " "
              << std::endl;

    real_type inci = n_hv.incidence_goals(S_MALE);

    std::cout << "hiv incidence goals M (%): t " << t << " " << 100.00 * inci
              << " " << std::endl;

    inci = n_hv.incidence_goals(S_FEMALE);

    std::cout << "hiv incidence goals F (%): t " << t << " " << 100.00 * inci
              << " " << std::endl;

    inci = n_hv.incidence_goals(S_ALL);

    std::cout << "hiv incidence goals (%): t " << t << " " << 100.00 * inci
              << " " << std::endl;

    real_type new_inf = n_hv.new_infections_goals(S_ALL);

    std::cout << "hiv new inf goals to dp: t " << t << " " << new_inf << " "
              << std::endl;

    new_inf = n_hv.total_new_infections;

    std::cout << "new inf goals lfg calc: t " << t << " " << new_inf << " " << std::endl;

    new_inf = n_hv.new_infections_dp;

    std::cout << "new inf dp aim: t " << t << " " << new_inf << " "
              << std::endl;

    real_type art_cov = 0.0;
    on_art = 0.0;
    for (int hd = CD4_GT500_ART; hd <= CD4_LT50_ART; ++hd) {
      on_art += n_hv.adults(VAC_ALL, RG_ALL, hd, S_ALL);
    }

    // real_type plhiv  =
    // n_hv.adults(VAC_ALL,RG_ALL,CD4_ALL,S_ALL)-n_hv.adults(VAC_ALL,RG_ALL,CD4_NEG,S_ALL);

    real_type plhiv = n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_MALE)
        + n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_FEMALE)
        - n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_MALE)
        - n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_FEMALE);

    if (plhiv > 0.0) {
      art_cov = on_art / plhiv;
    }

    std::cout << "art cov goals (%) t " << t << " " << 100.00 * art_cov << " "
              << std::endl;

    std::cout << "art number: t " << t << " " << on_art << " " << std::endl;

    std::cout << "plhiv number: t " << t << " " << plhiv << " " << std::endl;

    real_type prev = 0.0;

    if (n_hv.total_population > 0) {
      prev = plhiv / n_hv.total_population;
    }

    std::cout << "prev goals1 (%) t " << t << " " << 100.00 * prev << " "
              << std::endl;

    prev = n_hv.prevalence(RG_ALL, S_ALL);  // n_hv.total_prevalence;

    std::cout << "prev goals2 (%) t " << t << " " << 100.00 * prev << " "
              << std::endl;

    prev = n_hv.prevalence(RG_ALL, S_MALE);

    std::cout << "prev goals M (%) t " << t << " " << 100.00 * prev << " "
              << std::endl;

    prev = n_hv.prevalence(RG_ALL, S_FEMALE);

    std::cout << "prev goals F (%) t " << t << " " << 100.00 * prev << " "
              << std::endl;

    // plhiv = i_hv.pop_hivpos_adults + i_hv.total_pop_hivpos;
    // total_pop = i_hv.dp_totpop_1549(S_MALE) + i_hv.dp_totpop_1549(S_FEMALE);
    // prev = plhiv / total_pop;

    // std::cout << "prev dp/aim (%) t " << t << " " << 100.00 * prev << " "
    //           << std::endl;
  };

  void init_vars_pre_hiv_loop(int t) {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    nda::fill(i_hv.i_condom_prop, 0.0);  // input data, condom use, time t
    nda::fill(i_hv.i_num_partners, 0.0);  // input data, number of partners, time t
    nda::fill(i_hv.i_age_first_sex, 0.0);  // input data, age first sex, time t
    i_hv.i_idu_share_prop = 0.0;  // input data, idu sharing,, time t

    nda::fill(n_hv.deaths, 0.0);  // deaths for each t
    n_hv.total_deaths = 0.0;  // deaths for each t
    n_hv.total_deaths_hiv = 0.0;  // deaths for each t

    nda::fill(n_hv.new_infections, 0.0);  // new infections for each v, rg,  t
    n_hv.total_new_infections = 0.0;  // new infections for each  t

    nda::fill(n_hv.new_inf_s, 0.0);  // new infections for each t, by sex, for aim
    nda::fill(n_hv.new_infections_goals, 0.0);  // incidence for DP
    nda::fill(n_hv.incidence_goals, 0.0);

    n_hv.total_population = 0.0;
    n_hv.total_plhiv = 0.0;
    n_hv.total_on_art = 0.0;
    n_hv.total_new_vaccinations = 0.0;
    n_hv.total_vaccinated = 0.0;


    // CDP: temp, using i_hv change back to p_hv
    nda::fill(p_hv.art_coverage_rg, 0.0);

    // infectiousness not on art
    nda::fill(n_hv.mult_no_art, 0.0);
    n_hv.mult_no_art(CD4_PRIM) = p_hv.epi_infectiousness(INF_PRIM);  // INF_PRIM
    n_hv.mult_no_art(CD4_GT500) = p_hv.epi_infectiousness(INF_ASYMPT);  // INF_ASYMPT
    n_hv.mult_no_art(CD4_350_500) = p_hv.epi_infectiousness(INF_ASYMPT);  // INF_ASYMPT
    n_hv.mult_no_art(CD4_250_349) = p_hv.epi_infectiousness(INF_ASYMPT);  // INF_ASYMPT
    n_hv.mult_no_art(CD4_200_249) = p_hv.epi_infectiousness(INF_ASYMPT);  // INF_ASYMPT
    n_hv.mult_no_art(CD4_100_199) = p_hv.epi_infectiousness(INF_SYMPT_NO_ART);  // INF_SYMPT_NO_ART
    n_hv.mult_no_art(CD4_50_99) = p_hv.epi_infectiousness(INF_SYMPT_NO_ART);  // INF_SYMPT_NO_ART
    n_hv.mult_no_art(CD4_LT50) = p_hv.epi_infectiousness(INF_SYMPT_NO_ART);  // INF_SYMPT_NO_ART

    // infectiousness on art
    // CDP check CD4 dependency
    nda::fill(n_hv.mult_art, 0.0);
    for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
      n_hv.mult_art(hd) = p_hv.epi_inf_mult_art(t);  //* p_hv.epi_infectiousness(INF_SYMPT_NO_ART);
      if (t > p_hv.goals_base_year_idx) {
        n_hv.mult_art(hd) = p_hv.epi_inf_mult_art(t) *
                            p_hv.epi_infectiousness(INF_ASYMPT) *
                            (1.0 - p_hv.rn_poc_cov(POC_VL, t) * p_hv.rn_poc_effect(POC_VL));
      }
    }

    nda::fill(i_hv.prep_effect, 0.0);
    if (t > p_hv.goals_base_year_idx) {

      for (int s = S_MALE; s <= S_FEMALE; ++s) {
        for (int rg = RG_LRH; rg <= RG_TOTAL1; ++rg) {
          if (s == S_FEMALE && rg >= RG_MSM) {
            continue;
          }

          for (int m = PREP_OralDaily; m <= PREP_PEP; ++m) {
            real_type prep_effect = std::clamp(p_hv.prep_effectiveness(PREP_EFFECT, m), 0.0, 1.0);
            real_type prep_adh = std::clamp(p_hv.prep_effectiveness(PREP_ADH, m), 0.0, 1.0);
            real_type prep_methodmix = std::clamp(p_hv.prep_method_mix(s, rg, m, t), 0.0, 1.0);

            i_hv.prep_effect(rg, s) += prep_effect * prep_adh * prep_methodmix;

          }  // m
        }  // rg
      }  // s
    }

    // mort impact variables
    i_hv.AHD_Tx_Impact = 1.0;
    i_hv.alpha_mult = 1.0;

    // these behavioral parameters are adjusted by interventions via the impact
    // matrix. copies of the input parameters, which cant be changes, are made
    // for this prupose
    const int t_behav = std::min(t, p_hv.goals_base_year_idx);

    for (int rg = RG_NONE; rg <= RG_MSM_F3; ++rg) {
      i_hv.i_condom_prop(rg) = p_hv.b_condom_prop(rg, t_behav);
      i_hv.i_num_partners(rg) = p_hv.b_num_partners(rg, t_behav);
    }

    i_hv.i_condom_prop(RG_IDU) = p_hv.b_condom_prop(RG_MRH, t_behav);  // IDU has MR behavior

    i_hv.i_age_first_sex(S_MALE) = p_hv.b_age_first_sex(S_MALE, t_behav);
    i_hv.i_age_first_sex(S_FEMALE) = p_hv.b_age_first_sex(S_FEMALE, t_behav);

    i_hv.i_idu_share_prop = p_hv.b_idu_share_prop(t_behav);
  }

  void init_vars_hiv_loop() {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;

    nda::fill(n_hv.new_infections_goals, 0.0);  // new infections calculated for time step

    nda::fill(n_hv.newly_on_art, 0.0);  // art allocation for time step
    nda::fill(n_hv.prevalence, 0.0);  // prevalence for each rg,  t

    nda::fill(n_hv.new_inf_vrs, 0.0);  // new infections for each t, by v,r,s, for goals
    n_hv.total_new_vaccinations = 0.0;

    nda::fill(n_hv.cured_pop, 0.0);
    nda::fill(n_hv.cured_prop, 0.0);

    n_hv.total_prevalence = 0.0;  // total prevalence for each t

    nda::fill(i_hv.new_vaccinations, 0.0); //for each t, by v,r,s, for goals
    nda::fill(i_hv.vac_effect, 0.0);

    nda::fill(i_hv.cure_effect, 0.0);
  }

  void set_adults_hiv_loop() {
    auto& n_hv = state_next.hv;

    // init time dependent vars
    for (int v = VAC_UNV; v <= VAC_ALL; ++v) {
      for (int rg = RG_NONE; rg <= RG_ALL; ++rg) {
        for (int hd = CD4_NEG; hd <= CD4_ALL; ++hd) {
          for (int s = S_MALE; s <= S_ALL; ++s) {
            n_hv.adults_ts(v, rg, hd, s) = n_hv.adults(v, rg, hd, s);
          }
        }
      }
    }
  }

  // pre inner loop functions
  void set_riskgroup_proportions() {
    const auto& p_hv = pars.hv;
    auto& i_hv = intermediate.hv;

    nda::fill(i_hv.b_riskgroup_proportions, 0.0);  // risk group proportions
    nda::fill(i_hv.b_behave_change_rate, 0.0);  // risk group proportions

    // risk group proportions, and behaviour change rates
    for (int rg = RG_NONE; rg <= RG_MSM; ++rg) {  // CDP reset range

      // CDP note +1 on inputs
      i_hv.b_riskgroup_proportions(rg, S_MALE) = p_hv.b_behav_properties(rg, PERC_POP);
      if (rg > RG_LRH) {
        i_hv.b_behave_change_rate(rg, S_MALE) =
            (p_hv.b_behav_properties(rg, DUR_AVG) != 0.0)
            ? 1.0 / p_hv.b_behav_properties(rg, DUR_AVG) : 0.0;
        if (i_hv.b_behave_change_rate(rg, S_MALE) <= 0.001) {
          i_hv.b_behave_change_rate(rg, S_MALE) = 0.0;
        }
      }

      if (rg < RG_MSM) {
        // CDP note offset for women
        i_hv.b_riskgroup_proportions(rg, S_FEMALE) =
            p_hv.b_behav_properties(rg + RG_NONE_F3, PERC_POP);
        if (rg > RG_LRH) {
          i_hv.b_behave_change_rate(rg, S_FEMALE) =
              (p_hv.b_behav_properties(rg + RG_NONE_F3, DUR_AVG) != 0.0)
              ? 1.0 / p_hv.b_behav_properties(rg + RG_NONE_F3, DUR_AVG) : 0.0;
          if (i_hv.b_behave_change_rate(rg, S_FEMALE) <= 0.001) {
            i_hv.b_behave_change_rate(rg, S_FEMALE) = 0.0;
          }
        }
      }
    }
  }

  void set_initial_pop() {
    // dp
    const auto& c_dp = state_curr.dp;

    // goals
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;

    nda::fill(n_hv.adults, 0.0);
    nda::fill(n_hv.adults_ts, 0.0);
    nda::fill(i_hv.pop_1549, 0.0);

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      for (int a = SS::pIDX_15to49; a < SS::pIDX_15to49 + SS::pAG_15to49; ++a) {
        i_hv.pop_1549(s) += c_dp.p_totpop(a, s);
      }  // a

      for (int rg = RG_NONE; rg <= RG_TOTAL; ++rg) {
        n_hv.adults(VAC_UNV, rg, CD4_NEG, s) =
            i_hv.b_riskgroup_proportions(rg, s) * i_hv.pop_1549(s);
      }  // rg

    }  // s
  }

  void set_initial_pulse() {
    const auto& p_ha = pars.ha;
    auto& n_hv = state_next.hv;
    const auto& p_hv = pars.hv;

    real_type pulse = 0.0;
    real_type distr = 0.0;

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      for (int rg = RG_LRH; rg <= RG_TOTAL; ++rg) {
        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;
        }

        const int a25_34 = 10;  // of set from index 0=age 15

        pulse = p_hv.epi_initial_pulse * n_hv.adults(VAC_UNV, rg, CD4_NEG, s);
        n_hv.new_infections_goals(s) += pulse;

        if (rg == RG_MRH) {
          pulse = (3.0 / 5.0) * pulse;
        }

        if (rg == RG_LRH) {
          pulse = (1.0 / 5.0) * pulse;
        }

        if (rg == RG_NONE || rg == RG_IDU) {
          pulse = 0.0;
        }

        for (int hd = CD4_PRIM; hd <= CD4_LT50; ++hd) {
          int hd_hds = 0;  // set prim

          if (hd == CD4_GT500) {
            continue;  // see goals
          }

          if (hd >= CD4_350_500) {
            hd_hds = hd - CD4_GT500;  // CD4_350_500 and higher indices
          }

          distr = p_ha.cd4_initial_distribution(
              hd_hds, a25_34, s
          );  // CDP note the use of one age category for distr from AIM

          n_hv.adults(VAC_UNV, rg, hd, s) = n_hv.adults(VAC_UNV, rg, hd, s) + pulse * distr;
          n_hv.adults(VAC_UNV, rg, CD4_NEG, s) = n_hv.adults(VAC_UNV, rg, CD4_NEG, s) - pulse * distr;

          if (n_hv.adults(VAC_UNV, rg, CD4_NEG, s) < 0.0) {
            n_hv.adults(VAC_UNV, rg, CD4_NEG, s) = 0.0;
          }
        }
      }  // rg
    }  // s
  }

  void set_initial_pulse_idu() {
    const auto& p_ha = pars.ha;
    auto& n_hv = state_next.hv;
    const auto& p_hv = pars.hv;

    real_type pulse = 0.0;
    real_type distr = 0.0;

    int rg = RG_IDU;
    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      const int a25_34 = 10;  // of set from index 0=age 15

      pulse = p_hv.epi_initial_pulse * n_hv.adults(VAC_UNV, rg, CD4_NEG, s);
      n_hv.new_infections_goals(s) += pulse;

      for (int hd = CD4_PRIM; hd <= CD4_LT50; ++hd) {
        int hd_hds = 0;  // set prim

        if (hd == CD4_GT500) {
          continue;  // see goals
        }

        if (hd >= CD4_350_500) {
          hd_hds = hd - CD4_GT500;  // CD4_350_500 and higher indices
        }

        distr = p_ha.cd4_initial_distribution(
            hd_hds, a25_34, s
        );  // CDP note the use of one age category for distr from AIM

        n_hv.adults(VAC_UNV, rg, hd, s) =
            n_hv.adults(VAC_UNV, rg, hd, s) + pulse * distr;
        n_hv.adults(VAC_UNV, rg, CD4_NEG, s) =
            n_hv.adults(VAC_UNV, rg, CD4_NEG, s) - pulse * distr;

        if (n_hv.adults(VAC_UNV, rg, CD4_NEG, s) < 0.0) {
          n_hv.adults(VAC_UNV, rg, CD4_NEG, s) = 0.0;
        }

      }  // hd
    }  // s
  }

   void calc_vac_effect(int t) {
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    nda::fill(i_hv.vac_effect, 0.0);

    i_hv.vac_effect(VAC_TAKEACTION, VAC_TAKE) = p_hv.rn_vac_params(VAC_EFF);
    i_hv.vac_effect(VAC_TAKEACTION, VAC_PARTIAL) = 0.0;
    i_hv.vac_effect(VAC_TAKEACTION, VAC_NO_PROT) = 1.0 - p_hv.rn_vac_params(VAC_EFF);


    i_hv.vac_effect(VAC_DEGREEACTION, VAC_TAKE) = 0.0;
    i_hv.vac_effect(VAC_DEGREEACTION, VAC_PARTIAL) = 1.0;
    i_hv.vac_effect(VAC_DEGREEACTION, VAC_NO_PROT) = 0.0;

   }

  void calc_new_vaccinations(int t) {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    nda::fill(i_hv.new_vaccinations, 0.0);

    real_type vac_waning_rate =
    (p_hv.rn_vac_params(VAC_DUR) > 0.0) ? 1.0 / p_hv.rn_vac_params(VAC_DUR) : 0.0;

    real_type value = 0.0;
    real_type vac_coverage = 0.0;

    int nr = 0;
    real_type entrants_h = 0.0;
    real_type rg_denom = 0.0;
    real_type rg_ratio = 0.0;

    int h1 = CD4_LT50;
    if (p_hv.rn_vac_targetting == VAC_TARGET_HIV_NEG) {
      h1 = CD4_NEG;
    }

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      entrants_h = 0.0;
      rg_denom = 0.0;
      rg_ratio = 0.0;

      for (int hd = CD4_NEG; hd <= h1; ++hd) {
        if (p_hv.rn_vac_targetting == VAC_TARGET_HIV_NEG) {
          entrants_h += i_hv.dp_entrants_age_15(POP_H_HIVNeg, hd, s);
        } else {
          entrants_h += i_hv.dp_entrants_age_15(POP_H_HIVNeg, hd, s) +
                        i_hv.dp_entrants_age_15(POP_H_NoART, hd, s);
        }

        for (int rg = RG_NONE; rg <= RG_TOTAL1; ++rg) {
          if (s == S_FEMALE && rg >= RG_MSM) {
            continue;  // no female MSM
          }
          rg_denom += n_hv.adults(VAC_ALL, rg, hd, s);
        }
      }

      for (int rg = RG_NONE; rg <= RG_TOTAL1; ++rg) {

        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;  // no female MSM
        }

        int h1 = CD4_LT50;
        if (p_hv.rn_vac_targetting == VAC_TARGET_HIV_NEG) {
          h1 = CD4_NEG;
        }

        for (int hd = CD4_NEG; hd <= h1; ++hd) {
          // Determine the risk index (nr) used for coverage lookup
          nr = rg;
          if (s == S_FEMALE) {
            nr = rg + RG_NONE_F3;
          }

          vac_coverage = p_hv.rn_vac_coverage_all(t);
          if (p_hv.rn_vac_cov_type != VAC_COV_ALLRISK) {
            vac_coverage = p_hv.rn_vac_coverage_rg(nr, t);
          }

          vac_coverage = std::clamp(vac_coverage, 0.0, 1.0);

          rg_ratio = (rg_denom > 0.0) ? n_hv.adults(VAC_ALL, rg, hd, s) / rg_denom : 0.0;

          value = vac_coverage
                  * (n_hv.adults(VAC_ALL, rg, hd, s) +
                     // note it is assumed no one is vaccined below age 15
                     entrants_h * rg_ratio)
                    - (n_hv.adults(VAC_TAKE, rg, hd, s)
                      + n_hv.adults(VAC_PARTIAL, rg, hd, s)
                      + n_hv.adults(VAC_NO_PROT, rg, hd, s)) * (1.0 - vac_waning_rate);

          value = std::max(value, 0.0);

          i_hv.new_vaccinations(rg, hd, s) += value;
          n_hv.total_new_vaccinations += value;
        }

      }  // rg

    }  // s

  }

  // inner loop functions
  void set_goals_vars_from_dp(int t, int hiv_step) {
    // dp
    const auto& c_dp = state_curr.dp;
    const auto& n_dp = state_next.dp;
    const auto& i_dp = intermediate.dp;

    // aim
    const auto& c_ha = state_curr.ha;
    const auto& n_ha = state_next.ha;
    const auto& p_ha = pars.ha;

    //child model
    auto& n_hc = state_next.hc;


    // goals
    auto& i_hv = intermediate.hv;

    // vars mapped from DP/AIM
    nda::fill(
        i_hv.dp_totpop_deaths_background, 0.0
    );  // dp background deaths numerator
    // CDP review name
    nda::fill(i_hv.dp_totpop_1549, 0.0);  // dp background deaths denominator

    nda::fill(i_hv.dp_pop_1549, 0.0);  // denominator for background mortality, check age index (a-1)

    nda::fill(i_hv.dp_pop_1549_hiv, 0.0);  // denominator for goals mu
    nda::fill(i_hv.dp_pop_1549_art, 0.0);  // denominator for goals alpha

    nda::fill(i_hv.dp_hiv_cd4_mort_no_art, 0.0);  // numerator for  mu
    nda::fill(i_hv.dp_hiv_cd4_mort_art, 0.0);  // numerator for alpha
    nda::fill(i_hv.dp_hiv_cd4_progression, 0.0);  // numerator for lambda

    if (hiv_step == 0) {
      nda::fill(i_hv.dp_entrants_age_15, 0.0);  // numerator for 15 yr old entrants into goals
    }
    nda::fill(i_hv.dp_pop_sex_age_hiv, 0.0);  // used for ART allocation in
                                              // goals
    nda::fill(i_hv.dp_aging_50, 0.0);  // numerator for aging rate in goals, 50 yrs
    nda::fill(i_hv.dp_aging_denom_1549, 0.0);  // denominator for aging rate in goals


    // denominators for resource need
    i_hv.pop_hivpos_adults = 0.0;
    i_hv.pop_hivpos_children = 0.0;
    i_hv.pop_art_adults = 0.0;
    i_hv.pop_art_children = 0.0;

    i_hv.pop_hivpos_adults_ahd = 0.0;
    i_hv.pop_hivpos_children_ahd = 0.0;
    i_hv.pop_art_adults_ahd = 0.0;
    i_hv.pop_art_children_ahd = 0.0;

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
        for (int a = 0; a < pAG; ++a) {

          //adults
          const int a_hv = a - 15;  // for adult structure age 15 starts at 0
          if (a >= 15) {

            for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
              const int hd_hds = hd - CD4_GT500;
              i_hv.pop_hivpos_adults += n_ha.h_hivpop(hd_hds, a_hv, s);
              if(hd>=CD4_100_199){
                i_hv.pop_hivpos_adults_ahd += n_ha.h_hivpop(hd_hds, a_hv, s);
              }

              for (int ht = 0; ht < nART; ++ht) {
                i_hv.pop_art_adults += n_ha.h_artpop(ht, hd_hds, a_hv, s);
                if(hd>=CD4_100_199){
                  i_hv.pop_art_adults_ahd += n_ha.h_artpop(ht, hd_hds, a_hv, s);
                }
              }
            }//hd
         }
        else {

            if (a < hc2_agestart){
              for (int hd = 0; hd < hc1DS; ++hd) {
                 for (int cat = 0; cat < hcTT; ++cat) {
                    i_hv.pop_hivpos_children += n_hc.hc1_hivpop(hd, cat, a, s);
                  }
                  for (int dur = 0; dur < hTS; ++dur) {
                    i_hv.pop_art_children += n_hc.hc1_artpop(dur, hd, a, s);
                  }
              }
            }
            else{
              for (int hd = 0; hd < hc2DS; ++hd) {
                for (int cat = 0; cat < hcTT; ++cat) {
                  i_hv.pop_hivpos_children +=  n_hc.hc2_hivpop(hd, cat, a - hc2_agestart, s);
                }

                for (int dur = 0; dur < hTS; ++dur) {
                    i_hv.pop_art_children += n_hc.hc2_artpop(dur, hd, a - hc2_agestart, s);
                }

              }
            }
        }
      }
    }

    //set ahd in children to same ratio as in adults
    i_hv.pop_hivpos_children_ahd = (i_hv.pop_hivpos_adults > 0.0) ? (i_hv.pop_hivpos_adults_ahd/i_hv.pop_hivpos_adults)*
                                                                i_hv.pop_hivpos_children : 0.0;

    i_hv.pop_art_children_ahd = (i_hv.pop_art_adults > 0.0) ? (i_hv.pop_art_adults_ahd/i_hv.pop_art_adults)*
                                                           i_hv.pop_art_children : 0.0;



    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      int a50 = SS::pIDX_15to49 + SS::pAG_15to49;
      i_hv.dp_aging_50(POP_H_HIVNeg, CD4_NEG, s) += n_dp.p_totpop(a50, s);

      const int a50_hv = 35;  // for adult structure age 15 starts at 0
      for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
        const int hd_hds = hd - CD4_GT500;

        i_hv.dp_aging_50(POP_H_NoART, hd, s) += n_ha.h_hivpop(hd_hds, a50_hv, s);
        i_hv.dp_aging_50(POP_H_HIVNeg, CD4_NEG, s) -= n_ha.h_hivpop(hd_hds, a50_hv, s);

        for (int ht = 0; ht < nART; ++ht) {
          i_hv.dp_aging_50(POP_H_ARTlt6m + ht, hd, s) += n_ha.h_artpop(ht, hd_hds, a50_hv, s);
          i_hv.dp_aging_50(POP_H_OnART, hd, s) += n_ha.h_artpop(ht, hd_hds, a50_hv, s);
          i_hv.dp_aging_50(POP_H_HIVNeg, CD4_NEG, s) -= n_ha.h_artpop(ht, hd_hds, a50_hv, s);
        }
      }

      for (int a = SS::pIDX_15to49; a < SS::pIDX_15to49 + SS::pAG_15to49; ++a) {
        i_hv.dp_totpop_deaths_background(s) += n_dp.p_deaths_background_totpop(a, s);
        i_hv.dp_totpop_1549(s) += n_dp.p_totpop(a - 1, s);

        i_hv.dp_pop_1549(s) += n_dp.p_totpop(a, s);
        i_hv.dp_aging_denom_1549(POP_H_HIVNeg, CD4_NEG, s) += n_dp.p_totpop(a, s);
        if ((hiv_step == 0) && (a == SS::pIDX_15to49)) {
          i_hv.dp_entrants_age_15(POP_H_HIVNeg, CD4_NEG, s) += n_dp.p_totpop(a, s);
        }

        for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
          const int hd_hds = hd - CD4_GT500;
          const int a_hv = a - 15;  // for adult structure age 15 starts at 0

          i_hv.dp_pop_1549_hiv(hd, s) += n_ha.h_hivpop(hd_hds, a_hv, s);

          real_type hivpop_h = n_ha.h_hivpop(hd_hds, a_hv, s);
          real_type hivpop_h1 =0.0;

          i_hv.dp_pop_sex_age_hiv(POP_H_NoART, s) += hivpop_h;
          i_hv.dp_aging_denom_1549(POP_H_NoART, hd, s) += hivpop_h;
          i_hv.dp_aging_denom_1549(POP_H_HIVNeg, CD4_NEG, s) -= hivpop_h;  // remove hiv states

          if ((hiv_step == 0) && (a == SS::pIDX_15to49)) {
            i_hv.dp_entrants_age_15(POP_H_NoART, hd, s) += hivpop_h;
            i_hv.dp_entrants_age_15(POP_H_HIVNeg, CD4_NEG, s) -= hivpop_h;
          }

          i_hv.dp_hiv_cd4_mort_no_art(hd, s) += n_ha.h_hiv_deaths_no_art(hd_hds, a_hv, s);
          if (hd > CD4_GT500) {
            hivpop_h1 = n_ha.h_hivpop(hd_hds-1, a_hv, s);
            i_hv.dp_hiv_cd4_progression(hd - 1, s) += p_ha.cd4_progression(hd_hds-1, a_hv, s) * hivpop_h1;
          }

          for (int ht = 0; ht < nART; ++ht) {
            real_type artpop_h = n_ha.h_artpop(ht, hd_hds, a_hv, s);

            i_hv.dp_pop_1549_art(hd, s) += artpop_h;

            i_hv.dp_pop_sex_age_hiv(POP_H_ARTlt6m + ht, s) += artpop_h;
            i_hv.dp_aging_denom_1549(POP_H_ARTlt6m + ht, hd, s) += artpop_h;
            i_hv.dp_aging_denom_1549(POP_H_OnART, hd, s) += artpop_h;
            i_hv.dp_aging_denom_1549(POP_H_HIVNeg, CD4_NEG, s) -=
                artpop_h;  // remove art states

            // TODO: CDP: using an average rate. review
            // i_hv.art_alpha(hd,s) +=  (p_ha.art_mortality(ht, hd, a,
            // s)+p_ha.art_nonaids_excess_mort(ht, hd, a, s)) *
            // c_ha.h_artpop(ht, hd, a, s);

            if ((hiv_step == 0) && (a == SS::pIDX_15to49)) {
              i_hv.dp_entrants_age_15(POP_H_ARTlt6m + ht, hd, s) += artpop_h;
              i_hv.dp_entrants_age_15(POP_H_OnART, hd, s) += artpop_h;
              i_hv.dp_entrants_age_15(POP_H_HIVNeg, CD4_NEG, s) -= artpop_h;
            }

            i_hv.dp_hiv_cd4_mort_art(hd_hds, s) +=
                n_ha.h_hiv_deaths_art(ht, hd_hds, a_hv, s);

          }  // ht
        }  // h
      }  // a
    }  // s
  }

   void calc_migration_rate(int t) {
    const auto& p_dp = pars.dp;
    auto& i_dp = intermediate.dp;
    auto& n_dp = state_next.dp;

    auto& i_hv = intermediate.hv;

    real_type migration_num=0;
    real_type migration_denom=0;

    for (int s = S_MALE; s <= S_FEMALE; ++s) {

      // Migration for ages 15-49, by sex
      migration_num = 0.0;
      migration_denom = 0.0;
      i_hv.migration_rate(s) = 0.0;

      for (int a = SS::pIDX_15to49; a < SS::pIDX_15to49 + SS::pAG_15to49; ++a) {
        migration_num += p_dp.net_migration(a, s, t);
        migration_denom += n_dp.p_totpop(a, s);
      }

      i_hv.migration_rate(s) = (migration_denom != 0.0) ? migration_num / migration_denom : 0.0;
    }

  };

  void calc_goals_rates(int t, int hiv_step) {
    const auto& p_ha = pars.ha;

    const auto& p_hv = pars.hv;
    auto& i_hv = intermediate.hv;

    real_type denominator = 0.0;
    real_type numerator = 0.0;

    // demographical rates
    if (hiv_step == 0) {
      nda::fill(i_hv.background_death_rate, 0.0);  // dp total pop 1549
      nda::fill(i_hv.rate_aging_50, 0.0);  // mort rate, hiv on ART
    }

    // rates for HIV dynamics
    nda::fill(i_hv.hiv_mu, 0.0);  // mort rate, hiv not on ART
    nda::fill(i_hv.hiv_mu_excess, 0.0);  // non aids excess mortality

    nda::fill(i_hv.hiv_lambda, 0.0);  // progression rate, hiv not on ART
    nda::fill(i_hv.art_alpha, 0.0);  // mort rate, hiv on ART

    // aggregate rates
    nda::fill(i_hv.hiv_stage_progressors, 0.0);  // progression rate, hiv not on ART

    real_type years_in_primary = p_hv.epi_months_in_primary / 12.0;

    //using this switch to control if HIV parameters come from dp/aim's editor, or if
    //it is average from dp/aim population numbers. for now, forcing it as direct input
    bool do_cd4_params_editor = true;

    const int hOnArt = 7;
    const int a25_34 = 10;  // of set from index 0=age 15
    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      // demographical rate  dp_pop_1549 dp_totpop_1549
      if (hiv_step == 0) {
        i_hv.background_death_rate(s) = (i_hv.dp_pop_1549(s) != 0.0)
            ? i_hv.dp_totpop_deaths_background(s) / i_hv.dp_pop_1549(s) : 0.0;
      }


      // set aging rates, out of 15-49 year old population
      i_hv.rate_aging_50(POP_H_HIVNeg, CD4_NEG, s) =
          (i_hv.dp_aging_denom_1549(POP_H_HIVNeg, CD4_NEG, s) != 0.0)
          ? i_hv.dp_aging_50(POP_H_HIVNeg, CD4_NEG, s)
              / i_hv.dp_aging_denom_1549(POP_H_HIVNeg, CD4_NEG, s) : 0.0;

      // cd4 transition rate for primary stage
      i_hv.hiv_lambda(CD4_PRIM, s) =
          (years_in_primary != 0.0) ? 1.0 / years_in_primary : 0.0;

      for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
        const int hd_hds = hd - CD4_GT500;

        //not doing averaging in this version but taking inputs from the the dp/aim editor
        if (do_cd4_params_editor == true) {
          // direct from age categories, no averaging
          i_hv.hiv_mu(hd, s) = p_ha.cd4_mortality(hd_hds, a25_34, s);
          //ahd impact on mortality applies to advanced disease
          if(hd>=CD4_100_199){
            i_hv.hiv_mu(hd, s) *= i_hv.AHD_Tx_Impact;
          }

          i_hv.hiv_mu_excess(hd, s) = p_ha.cd4_nonaids_excess_mort(hd_hds, a25_34, s);
          i_hv.hiv_mu(hd, s) = i_hv.hiv_mu(hd, s) + i_hv.hiv_mu_excess(hd, s);
          i_hv.hiv_mu(hd, s) = std::max(i_hv.hiv_mu(hd, s), i_hv.background_death_rate(s));

          i_hv.hiv_lambda(hd, s) = p_ha.cd4_progression(hd_hds, a25_34, s);

          i_hv.art_alpha(hd + hOnArt, s) = i_hv.alpha_mult
              * p_ha.art_mortality_time_rate_ratio(ART_GT12m, t)
              * p_ha.art_mortality(ART_GT12m, hd_hds, a25_34, s);

          //ahd impact on mortality applies to advanced disease
          if(hd>=CD4_100_199){
            i_hv.art_alpha(hd + hOnArt, s) *= i_hv.AHD_Tx_Impact;
          }

          i_hv.art_alpha_excess(hd + hOnArt, s) = p_ha.art_nonaids_excess_mort(ART_GT12m, hd_hds, a25_34, s);
          i_hv.art_alpha(hd + hOnArt, s) = i_hv.art_alpha(hd + hOnArt, s) + i_hv.art_alpha_excess(hd + hOnArt, s);

          if (hd == CD4_GT500) {
            real_type stage_duration = 1.0 / i_hv.hiv_lambda(CD4_GT500, s) - years_in_primary;
            i_hv.hiv_lambda(CD4_GT500, s) = (1.0 - exp(-opts.dt / stage_duration)) / opts.dt;  // remove time in primary stage
            i_hv.hiv_mu(CD4_PRIM, s) = i_hv.hiv_mu(CD4_GT500, s);
          }

          if (hd == CD4_LT50) {
            i_hv.hiv_lambda(hd, s) = 0.0;  // no progession out of this stage
          }
        } else {
          // average mortality and progression rates
          i_hv.hiv_mu(hd, s) = (i_hv.dp_pop_1549_hiv(hd, s) != 0.0)
              ? i_hv.dp_hiv_cd4_mort_no_art(hd, s) / i_hv.dp_pop_1549_hiv(hd, s)
              : p_ha.cd4_mortality(hd_hds, 25, s);  // using age 25 for defaults
          i_hv.hiv_mu_excess(hd, s) =
              p_ha.cd4_nonaids_excess_mort(hd_hds, a25_34, s);
          i_hv.hiv_mu(hd, s) = i_hv.hiv_mu(hd, s) + i_hv.hiv_mu_excess(hd, s);
          i_hv.hiv_mu(hd, s) = std::max(i_hv.hiv_mu(hd, s), i_hv.background_death_rate(s));

          i_hv.hiv_lambda(hd, s) = (i_hv.dp_pop_1549_hiv(hd, s) != 0.0)
              ? i_hv.dp_hiv_cd4_progression(hd, s) / i_hv.dp_pop_1549_hiv(hd, s)
              : p_ha.cd4_progression(hd_hds, 25, s);  //

          i_hv.art_alpha(hd + hOnArt, s) = (i_hv.dp_pop_1549_art(hd, s) != 0.0)
              ? i_hv.dp_hiv_cd4_mort_art(hd, s) / i_hv.dp_pop_1549_art(hd, s)
              : p_ha.art_mortality(2, hd_hds, 25, s);  // 2 is gt12 m on ART
          i_hv.art_alpha(hd + hOnArt, s) = i_hv.alpha_mult
              * p_ha.art_mortality_time_rate_ratio(ART_GT12m, t)
              * i_hv.art_alpha(hd + hOnArt, s);
          i_hv.art_alpha_excess(hd + hOnArt, s) =
              p_ha.art_nonaids_excess_mort(ART_GT12m, hd_hds, a25_34, s);
          i_hv.art_alpha(hd + hOnArt, s) = i_hv.art_alpha(hd + hOnArt, s)
              + i_hv.art_alpha_excess(hd + hOnArt, s);

          if (hd == CD4_GT500) {
            real_type stage_duration =
                1.0 / i_hv.hiv_lambda(CD4_GT500, s) - years_in_primary;
            i_hv.hiv_lambda(CD4_GT500, s) = (1.0 - exp(-opts.dt / stage_duration))
                / opts.dt;  // remove time in primary stage
            i_hv.hiv_mu(CD4_PRIM, s) = i_hv.hiv_mu(CD4_GT500, s);
          }

          if (hd == CD4_LT50) {
            i_hv.hiv_lambda(hd, s) = 0.0;  // no progession out of this stage
          }
        }

        i_hv.rate_aging_50(POP_H_NoART, hd, s) =
            (i_hv.dp_aging_denom_1549(POP_H_NoART, hd, s) != 0.0)
            ? i_hv.dp_aging_50(POP_H_NoART, hd, s)
                / i_hv.dp_aging_denom_1549(POP_H_NoART, hd, s) : 0.0;
        if (hd == CD4_GT500) {
          i_hv.rate_aging_50(POP_H_NoART, CD4_PRIM, s) =
              i_hv.rate_aging_50(POP_H_NoART, hd, s);
        }

        denominator = 0.0;
        numerator = 0.0;
        for (int ht = 0; ht < nART; ++ht) {
          numerator += i_hv.dp_aging_50(POP_H_ARTlt6m + ht, hd, s);
          denominator += i_hv.dp_aging_denom_1549(POP_H_ARTlt6m + ht, hd, s);
        }
        i_hv.rate_aging_50(POP_H_OnART, hd, s) =
            (denominator != 0.0) ? numerator / denominator : 0.0;
      }
    }

  }

  void calc_HIV_mort_adjustments(int t
  ) {  // Reduce mortality following treatment for advanced HIV disease

    const auto& p_ha = pars.ha;

    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    real_type AHD_cov = 0.0;
    int GoalsBaseYearIdx = p_hv.goals_base_year_idx;

    i_hv.AHD_Tx_Impact = 1.0;
    if (t > GoalsBaseYearIdx) {
      AHD_cov = p_hv.rn_ahd_treat_cov(t) * (1.0 + p_hv.rn_poc_cov(POC_CD4, t) * p_hv.rn_poc_effect(POC_CD4));
      AHD_cov = std::clamp(AHD_cov, 0.0, 1.0);

      i_hv.AHD_Tx_Impact = 1.0 - (AHD_cov - p_hv.rn_ahd_treat_cov(GoalsBaseYearIdx)) * p_hv.rn_ahd_treat_reduc_mort;
      i_hv.AHD_Tx_Impact = std::clamp(i_hv.AHD_Tx_Impact, 0.0, 1.0);
    }

    // Reduce mortality by ART by 50% as viral suppression increases to 95%
    // Input is percent not virally suppressed, initial default is 0.25
    // Fast-Track target is 0.05
    i_hv.alpha_mult = 1 - std::min(0.5, 0.5 * (p_hv.epi_inf_mult_art(1)
                          - p_hv.epi_inf_mult_art(t))
                       / (p_hv.epi_inf_mult_art(1) - 0.05));

    if(t > p_hv.goals_base_year_idx){
     i_hv.alpha_mult = 1 - std::min(0.5, 0.5 * (p_hv.epi_inf_mult_art(1)
                          - p_hv.epi_inf_mult_art(t)
                              * (1.0 - p_hv.rn_poc_cov(POC_VL, t) * p_hv.rn_poc_effect(POC_VL))
                              * (1.0 - p_hv.long_act_treat_cov(t) * p_hv.long_act_treat_eff_vls))
                       / (p_hv.epi_inf_mult_art(1) - 0.05));
    }

    i_hv.alpha_mult = std::clamp(i_hv.alpha_mult, 0.0, 1.0);
  }

   void calc_HIV_cure_coverage(int t){

    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    int nr=0;
    real_type hv_plhiv = 0.0;
    real_type hv_plhiv_rg = 0.0;

    i_hv.cure_effect(S_MALE)   = p_hv.rn_cure_coverage_all(t) * p_hv.rn_cure_effect(VAC_EFF);
    i_hv.cure_effect(S_FEMALE) = p_hv.rn_cure_coverage_all(t) * p_hv.rn_cure_effect(VAC_EFF);
    if (p_hv.rn_cure_coverage_type != CURE_COV_ALLRISK) {

        i_hv.cure_effect(S_MALE)   = 0.0;
        hv_plhiv =   n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_MALE) -
                     n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_MALE);

        for (int r = RG_NONE; r <= RG_MSM; ++r) {
            hv_plhiv_rg = n_hv.adults(VAC_ALL, r, CD4_ALL, S_MALE) -
                          n_hv.adults(VAC_ALL, r, CD4_NEG, S_MALE);

            i_hv.cure_effect(S_MALE) += (hv_plhiv > 0.0) ? (hv_plhiv_rg / hv_plhiv_rg) : 0.0 *
                                    p_hv.rn_cure_coverage_rg(r, t) *
                                    p_hv.rn_cure_effect(VAC_EFF);
        }

        i_hv.cure_effect(S_MALE) = std::clamp(i_hv.cure_effect(S_MALE), 0.0, 1.0);

        i_hv.cure_effect(S_FEMALE)   = 0.0;
        hv_plhiv =   n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_FEMALE) -
                     n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_FEMALE);

        for (int r = RG_NONE; r <= RG_MSM; ++r) {
           nr = r + RG_NONE_F3;

           hv_plhiv_rg = n_hv.adults(VAC_ALL, r, CD4_ALL, S_FEMALE) -
                         n_hv.adults(VAC_ALL, r, CD4_NEG, S_FEMALE);

           i_hv.cure_effect(S_FEMALE) += (hv_plhiv > 0.0) ? (hv_plhiv_rg / hv_plhiv) : 0.0 *
                                    p_hv.rn_cure_coverage_rg(nr, t) *
                                    p_hv.rn_cure_effect(VAC_EFF);
        }

        i_hv.cure_effect(S_FEMALE) = std::clamp(i_hv.cure_effect(S_FEMALE), 0.0, 1.0);

    }

  }

  void calc_HIV_cure(int t) {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;
    real_type curePercent = 0.0;
    real_type cured = 0.0;

    nda::fill(n_hv.pop_hiv_neg, 0.0);
    nda::fill(n_hv.cured_pop, 0.0);
    nda::fill(n_hv.cured_prop, 0.0);

    real_type cureDuration = p_hv.rn_cure_effect(VAC_DUR);
    real_type immuneWane =
        (cureDuration > 0.0) ? (1.0 / cureDuration) : (1.0 / 9.0);

    int nr = 0;

    for (int rg = RG_NONE; rg <= RG_MSMIDU; ++rg) {
      for (int s = S_MALE; s <= S_FEMALE; ++s) {
        for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
          if (!((s == S_FEMALE) && (rg >= RG_MSM) && (rg <= RG_MSMIDU)))
          {  // Exclude female MSM and higher risk groups
            n_hv.pop_hiv_neg(rg, s) += n_hv.adults(v, rg, CD4_NEG, s);
          }
        }  // v

        // Determine the risk index (nr) used for coverage lookup
        nr = rg;
        if (s == S_FEMALE) {
          nr = rg + RG_NONE_F3;
        }

        curePercent = p_hv.rn_cure_coverage_all(t) * p_hv.rn_cure_effect(VAC_EFF);
        if (p_hv.rn_cure_coverage_type != CURE_COV_ALLRISK) {
          curePercent = p_hv.rn_cure_coverage_rg(nr, t) * p_hv.rn_cure_effect(VAC_EFF);
        }

        curePercent = std::clamp(curePercent, 0.0, 1.0);

        if (curePercent > 0.0) {
          for (int hd = CD4_GT500; hd <= CD4_LT50_ART; ++hd) {
            for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
              cured = n_hv.adults(v, rg, hd, s) * curePercent;  // opts.dt *

              // do not remove more than 99 % of the current compartment >> CDP
              // check
              cured = std::min(cured, 0.99 * n_hv.adults(v, rg, hd, s));

              n_hv.cured_pop(rg, s) += cured;

              // move cured individuals to the HIV‑negative compartment
              if (n_hv.adults(v, rg, hd, s) - cured > 0) {
                n_hv.adults(v, rg, hd, s) -= cured;
                n_hv.adults(v, rg, CD4_NEG, s) += cured;
              }

            }  // hd,v
          }

        }  // if

        // Update cured‑population accounting for natural death and waning
        // immunity
        n_hv.cured_pop(rg, s) -= (i_hv.background_death_rate(s) + immuneWane) * n_hv.cured_pop(rg, s);  // opts.dt *
        if (n_hv.cured_pop(rg, s) < 0.0) {
          n_hv.cured_pop(rg, s) = 0.0;
        }

        ///  Record the proportion of cured individuals among the HIV‑negative
        //  population (used later in force‑of‑infection calculations)
        if (n_hv.pop_hiv_neg(rg, s) > 0.0) {
          n_hv.cured_prop(rg, s) = n_hv.cured_pop(rg, s) / n_hv.pop_hiv_neg(rg, s);
        }

      }  // rg,s
    }
  }

  void calc_newrecruits_distribution(int t, int hiv_step) {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    real_type value = 0.0;
    real_type rg_sum = 0.0;
    real_type rg_proportion = 0.0;
    real_type debut_rate = 0.0;

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      if (n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, s) == 0.0) {
        continue;
      }

      rg_sum = 0.0;
      for (int rg = RG_LRH; rg <= RG_TOTAL1; ++rg) {
        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;
        }

        rg_proportion = p_hv.b_behav_properties(rg, PERC_POP);
        if (s == S_FEMALE) {
          rg_proportion = p_hv.b_behav_properties(rg + RG_NONE_F3, PERC_POP);
        }

        if (rg_proportion -
            n_hv.adults(VAC_ALL, rg, CD4_ALL, s) / n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, s) != 0.0)
        {
          debut_rate = (std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) != 0.0)
              ? 1.0 / std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) : 1.0;
          if (n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, s) == 0.0
              || i_hv.b_behave_change_rate(rg, s) == 0.0
              || debut_rate * n_hv.adults(VAC_ALL, RG_NONE, CD4_NEG, s) == 0.0)
          {
            value = 0.0;
          } else {
            debut_rate = (std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) != 0.0)
                ? 1.0 / std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) : 1.0;
            value = rg_proportion
                + (rg_proportion
                   - (1.0 - i_hv.b_behave_change_rate(rg, s))
                       * (n_hv.adults(VAC_ALL, rg, CD4_ALL, s)
                          / n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, s)))
                    * p_hv.b_incr_recruit(s, rg)
                    * n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, s)
                    / (debut_rate * n_hv.adults(VAC_ALL, RG_NONE, CD4_NEG, s));

            value = std::max(value, 0.0);
            i_hv.b_riskgroup_proportions(rg, s) = value;
          }

          rg_sum += i_hv.b_riskgroup_proportions(rg, s);
        }

      }  // rg

      // normalize
      for (int rg = RG_LRH; rg <= RG_TOTAL1; ++rg) {
        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;
        }

        if (rg_sum > 0.0) {
          i_hv.b_riskgroup_proportions(rg, s) /= rg_sum;
        }
      }

    }  // s
  }

  void progress_norisk_hiv_neg(int t, int hiv_step) {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    real_type value = 0.0;

    // unvaccinated
    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      value = n_hv.adults(VAC_UNV, RG_NONE, CD4_NEG, s)
          + opts.dt
              * ( // new entrants 15 yrs , note that they are not vaccinated by
                  // assumption
                  i_hv.dp_entrants_age_15(POP_H_HIVNeg, CD4_NEG, s) -

                  // exits
                  n_hv.adults_ts(VAC_UNV, RG_NONE, CD4_NEG, s)
                      * (((std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) != 0.0)
                              ? 1  / std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) : 0.0)
                         + i_hv.background_death_rate(s)
                         + i_hv.rate_aging_50(POP_H_HIVNeg, CD4_NEG, s)) +

                  // net migration
                  n_hv.adults_ts(VAC_UNV, RG_NONE, CD4_NEG, s)
                      * i_hv.migration_rate(s) -

                  // new vaccinations, out
                  i_hv.new_vaccinations(RG_NONE, CD4_NEG, s) +

                  // vaccine waning, entrants
                  (n_hv.adults_ts(VAC_TAKE, RG_NONE, CD4_NEG, s)
                   + n_hv.adults_ts(VAC_PARTIAL, RG_NONE, CD4_NEG, s)
                   + n_hv.adults_ts(VAC_NO_PROT, RG_NONE, CD4_NEG, s))
                      * ((i_hv.vac_params(VAC_DUR) != 0.0)
                             ? 1.0 / i_hv.vac_params(VAC_DUR) : 0.0)
              );

      value = std::max(value, 0.0);

      n_hv.adults(VAC_UNV, RG_NONE, CD4_NEG, s) = value;

      // keep track of deaths
      n_hv.deaths(VAC_UNV, RG_NONE, CD4_NEG, s) += opts.dt
          * n_hv.adults_ts(VAC_UNV, RG_NONE, CD4_NEG, s)
          * i_hv.background_death_rate(s);

      n_hv.total_deaths += opts.dt
          * n_hv.adults_ts(VAC_UNV, RG_NONE, CD4_NEG, s)
          * i_hv.background_death_rate(s);

     // vaccinated
      for (int v = VAC_TAKE; v <= VAC_NO_PROT; ++v) {
        value = n_hv.adults(v, RG_NONE, CD4_NEG, s)
            + opts.dt
                * (-
                    // exits
                    n_hv.adults_ts(v, RG_NONE, CD4_NEG, s)
                    * (i_hv.background_death_rate(s)
                      + i_hv.rate_aging_50(POP_H_HIVNeg, CD4_NEG, s)) +

                    // net migrationvac_effect
                      n_hv.adults_ts(v, RG_NONE, CD4_NEG, s)
                          * i_hv.migration_rate(s) +

                   // new vaccinations, in
                   i_hv.new_vaccinations(RG_NONE, CD4_NEG, s)
                      * i_hv.vac_effect(p_hv.rn_vac_type, v) -

                   // vaccine, exits
                   n_hv.adults_ts(v, RG_NONE, CD4_NEG, s)
                       * ((i_hv.vac_params(VAC_DUR) != 0.0)
                              ? 1.0 / i_hv.vac_params(VAC_DUR) : 0.0));

        value = std::max(value, 0.0);

        n_hv.adults(v, RG_NONE, CD4_NEG, s) = value;

        // keep track of deaths
        n_hv.deaths(v, RG_NONE, CD4_NEG, s) += opts.dt
            * n_hv.adults_ts(v, RG_NONE, CD4_NEG, s)
            * i_hv.background_death_rate(s);

        n_hv.total_deaths += opts.dt * n_hv.adults_ts(v, RG_NONE, CD4_NEG, s)
            * i_hv.background_death_rate(s);

      }  // v

    }  // s
  }

  void progress_atrisk_hiv_neg(int t, int RG_MIN, int RG_MAX) {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    real_type value = 0.0;

    // unvaccinated
    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      for (int rg = RG_MIN; rg <= RG_MAX; ++rg) {
        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;
        }

        value = n_hv.adults(VAC_UNV, rg, CD4_NEG, s)
            + opts.dt
                * (// no risk at sexual debut
                    n_hv.adults_ts(VAC_UNV, RG_NONE, CD4_NEG, s)
                        * ((std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) != 0.0)
                               ? 1.0 / std::max( i_hv.i_age_first_sex(s) - 15.0, 1.0 ) : 0.0)
                        * i_hv.b_riskgroup_proportions(rg, s) -

                    // exits
                    n_hv.adults_ts(VAC_UNV, rg, CD4_NEG, s)
                        * (i_hv.background_death_rate(s)
                           + i_hv.rate_aging_50(POP_H_HIVNeg, CD4_NEG, s)
                           +  // aging out at age 50
                           ((RG_MRH <= rg && rg <= RG_IDU)
                                ? i_hv.b_behave_change_rate(rg, s) : 0.0)) +

                    // net migration
                    n_hv.adults_ts(VAC_UNV, rg, CD4_NEG, s)
                        * i_hv.migration_rate(s) -

                    // new vaccinations
                    i_hv.new_vaccinations(rg, CD4_NEG, s) +

                    // vaccine waning, entrants
                    (n_hv.adults_ts(VAC_TAKE, rg, CD4_NEG, s)
                     + n_hv.adults_ts(VAC_PARTIAL, rg, CD4_NEG, s)
                     + n_hv.adults_ts(VAC_NO_PROT, rg, CD4_NEG, s))
                        * ((i_hv.vac_params(VAC_DUR) != 0.0)
                               ? 1.0 / i_hv.vac_params(VAC_DUR) : 0.0)
                    +

                    // entrants following behavior change
                    ((RG_LRH <= rg && rg <= RG_HRH)
                         ? n_hv.adults_ts(VAC_UNV, rg + 1, CD4_NEG, s)
                             * i_hv.b_behave_change_rate(rg + 1, s) : 0.0)
                );

        value = std::max(value, 0.0);

        n_hv.adults(VAC_UNV, rg, CD4_NEG, s) = value;

        // keep track of deaths
        n_hv.deaths(VAC_UNV, rg, CD4_NEG, s) += opts.dt
            * n_hv.adults_ts(VAC_UNV, rg, CD4_NEG, s)
            * i_hv.background_death_rate(s);

        n_hv.total_deaths += opts.dt * n_hv.adults_ts(VAC_UNV, rg, CD4_NEG, s)
            * i_hv.background_death_rate(s);

        // vaccinated
        for (int v = VAC_TAKE; v <= VAC_NO_PROT; ++v) {
          value = n_hv.adults(v, rg, CD4_NEG, s)
              + opts.dt
                  * (-
                    // exits and migration
                    n_hv.adults_ts(v, rg, CD4_NEG, s)
                    * (i_hv.background_death_rate(s)
                      + i_hv.rate_aging_50(POP_H_HIVNeg, CD4_NEG, s)
                    -  // aging out at age 50
                    ((RG_MRH <= rg && rg <= RG_IDU)
                          ? i_hv.b_behave_change_rate(rg, s) : 0.0)) +

                    // net migration
                    n_hv.adults_ts(v, rg, CD4_NEG, s)
                        * i_hv.migration_rate(s) +

                     // new vaccinations
                     i_hv.new_vaccinations(rg, CD4_NEG, s)
                          * i_hv.vac_effect(p_hv.rn_vac_type, v) -

                     // vaccine, exits
                     n_hv.adults_ts(v, rg, CD4_NEG, s)
                         * ((i_hv.vac_params(VAC_DUR) != 0.0)
                                ? 1.0 / i_hv.vac_params(VAC_DUR) : 0.0) +

                     // entrants following behavior change
                     ((RG_LRH <= rg && rg <= RG_HRH)
                          ? n_hv.adults_ts(v, rg + 1, CD4_NEG, s)
                              * i_hv.b_behave_change_rate(rg + 1, s) : 0.0));

          value = std::max(value, 0.0);

          n_hv.adults(v, rg, CD4_NEG, s) = value;

          // keep track of deaths
          n_hv.deaths(v, rg, CD4_NEG, s) += opts.dt
              * n_hv.adults_ts(v, rg, CD4_NEG, s)
              * i_hv.background_death_rate(s);

          n_hv.total_deaths += opts.dt * n_hv.adults_ts(v, rg, CD4_NEG, s)
              * i_hv.background_death_rate(s);

        }  // v

      }  // rg

    }  // s
  }

  void progress_hivp_and_art(int t, int RG_MIN, int RG_MAX) {
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    real_type dt = opts.dt;
    real_type value = 0.0;

    // unvaccinated
    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      for (int rg = RG_MIN; rg <= RG_MAX; ++rg) {
        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;
        }

        int hd_lb = CD4_LT50;
        if (t >= opts.ts_art_start) {
          hd_lb = CD4_LT50_ART;
        }

        const int hOnArt = 7;
        for (int hd = CD4_PRIM; hd <= hd_lb; ++hd) {
          if (hd >= CD4_GT500_ART && t < opts.ts_art_start) {
            continue;
          }

          // Mortality
          // hiv-pos or hiv-art mortality rate
          real_type mort_hiv = 0.0;
          if (CD4_PRIM <= hd && hd <= CD4_LT50) {
            mort_hiv = i_hv.hiv_mu(hd, s);
          } else {
            mort_hiv = i_hv.art_alpha(hd, s);
          };

          // Entrants 15 years from and DP and Aging out rate
          real_type dp_entrants_age_15 = 0.0;
          real_type rate_aging_out = 0.0;

          if (CD4_PRIM <= hd && hd <= CD4_LT50) {
            rate_aging_out = i_hv.rate_aging_50(POP_H_NoART, hd, s);
            if (rg == RG_NONE) {
              dp_entrants_age_15 = i_hv.dp_entrants_age_15(POP_H_NoART, hd, s);
            }
          } else {
            rate_aging_out = i_hv.rate_aging_50(POP_H_OnART, hd - hOnArt, s);
            if (rg == RG_NONE) {
              dp_entrants_age_15 =
                  i_hv.dp_entrants_age_15(POP_H_OnART, hd - hOnArt, s);
            }
          };

          // Progression rate
          // hiv-pos cd 4 progression rate, not progression on art
          real_type progress_out = 0.0;
          if (CD4_PRIM <= hd && hd <= CD4_LT50) {
            progress_out = i_hv.hiv_lambda(hd, s);
          };

          real_type progress_in = 0.0;
          if (CD4_GT500 <= hd && hd <= CD4_LT50) {
            progress_in = i_hv.hiv_lambda(hd - 1, s);
          };

          value = n_hv.adults(VAC_UNV, rg, hd, s)
              + opts.dt
                  * ( // new entrants 15 yrs , note that they are not vaccinated
                      // by assumption
                      dp_entrants_age_15 +

                      // no risk at sexual debut
                      n_hv.adults_ts(VAC_UNV, RG_NONE, hd, s)
                          * ((std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0)
                              != 0.0)
                                 ? 1.0 / std::max(i_hv.i_age_first_sex(s) - 15.0, 1.0) : 0.0)
                          * i_hv.b_riskgroup_proportions(rg, s) -

                      // exits
                      n_hv.adults_ts(VAC_UNV, rg, hd, s)
                          * (i_hv.background_death_rate(s) + mort_hiv
                             +  // hiv mortality rate
                             progress_out +  // hiv progrssion rate
                             rate_aging_out +  // aging out at age 50
                             ((RG_MRH <= rg && rg <= RG_IDU)
                                  ? i_hv.b_behave_change_rate(rg, s) : 0.0)) +

                      // net migration
                      n_hv.adults_ts(VAC_UNV, rg, hd, s)
                          * i_hv.migration_rate(s) -

                      // new vaccinations
                      i_hv.new_vaccinations(rg, hd, s) +

                      // vaccine waning, entrants
                      (n_hv.adults_ts(VAC_TAKE, rg, hd, s)
                       + n_hv.adults_ts(VAC_PARTIAL, rg, hd, s)
                       + n_hv.adults_ts(VAC_NO_PROT, rg, hd, s))
                          * ((i_hv.vac_params(VAC_DUR) != 0.0)
                                 ? 1.0 / i_hv.vac_params(VAC_DUR) : 0.0) +

                      // entrants following behavior change
                      ((RG_LRH <= rg && rg <= RG_HRH)
                           ? n_hv.adults_ts(VAC_UNV, rg + 1, hd, s)
                               * i_hv.b_behave_change_rate(rg + 1, s) : 0.0) +

                      // entrants following hiv stage progression
                      n_hv.adults_ts(VAC_UNV, rg, hd - 1, s) * progress_in
                  );

          value = std::max(value, 0.0);

          n_hv.adults(VAC_UNV, rg, hd, s) = value;

          // keep track of deaths
          n_hv.deaths(VAC_UNV, rg, hd, s) += opts.dt
              * n_hv.adults_ts(VAC_UNV, rg, hd, s)
              * (i_hv.background_death_rate(s) + mort_hiv);

          n_hv.total_deaths += opts.dt * n_hv.adults_ts(VAC_UNV, rg, hd, s)
              * (i_hv.background_death_rate(s) + mort_hiv);

          n_hv.total_deaths_hiv +=
              opts.dt * n_hv.adults_ts(VAC_UNV, rg, hd, s) * (mort_hiv);

          // vaccinated
          for (int v = VAC_TAKE; v <= VAC_NO_PROT; ++v) {
            value = n_hv.adults_ts(v, rg, hd, s)
                + opts.dt
                    * (-// exits
                        n_hv.adults_ts(v, rg, hd, s)
                        * (i_hv.background_death_rate(s) + mort_hiv
                          +  // hiv mortality rate
                          progress_out +  // hiv progrssion rate
                          rate_aging_out) +

                       // net migration
                       n_hv.adults_ts(v, rg, hd, s) * i_hv.migration_rate(s) +

                       // new vaccinations
                       i_hv.new_vaccinations(rg, hd, s)
                            * i_hv.vac_effect(p_hv.rn_vac_type, v) -

                       // vaccine, exits
                       n_hv.adults_ts(v, rg, hd, s)
                           * ((i_hv.vac_params(VAC_DUR) != 0.0)
                                  ? 1.0 / i_hv.vac_params(VAC_DUR) : 0.0) +

                       // entrants following behavior change
                       ((RG_LRH <= rg && rg <= RG_HRH)
                            ? n_hv.adults_ts(v, rg + 1, hd, s)
                                * i_hv.b_behave_change_rate(rg + 1, s) : 0.0) +

                       // entrants following hiv stage progression
                       n_hv.adults_ts(v, rg, hd, s) * progress_in);

            value = std::max(value, 0.0);

            n_hv.adults(v, rg, hd, s) = value;

            // keep track of deaths
            n_hv.deaths(v, rg, hd, s) += opts.dt * n_hv.adults_ts(v, rg, hd, s)
                * (i_hv.background_death_rate(s) + mort_hiv);

            n_hv.total_deaths += opts.dt * n_hv.adults_ts(v, rg, hd, s)
                * (i_hv.background_death_rate(s) + mort_hiv);

            n_hv.total_deaths_hiv +=
                opts.dt * n_hv.adults_ts(v, rg, hd, s) * (mort_hiv);

          }  // v

        }  // hd

      }  // rg

    }  // s
  }

  void calc_r_multiplier(int t) {
    // from goals
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    const real_type rMultEqlb = 1.7;  // 2.44 in previous version of Goals
    real_type rMultNumerator = 0.0;
    real_type rMultDenominator = 0.0;
    real_type rMultNumeratorAll = 0.0;
    real_type rMultDenominatorAll = 0.0;

    const int hOnArt = 7;

    // CDP check go  als calc
    real_type vaccine_factor = 1.0 - p_hv.rn_vac_params(VAC_INF)
              * (n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_ALL) - n_hv.adults(VAC_UNV, RG_ALL, CD4_ALL, S_ALL))
              / n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_ALL);

    nda::fill(i_hv.r_mult, 1.0);  // default multiplier

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      for (int rg = RG_LRH; rg <= RG_HRH; ++rg) {

        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;
        }

        rMultNumerator = 0.0;
        rMultDenominator = 0.0;

        // not on ART
        for (int hd = CD4_PRIM; hd <= CD4_LT50; ++hd) {
          rMultNumerator += n_hv.mult_no_art(hd) * n_hv.adults(VAC_ALL, rg, hd, s);
          rMultDenominator += n_hv.adults(VAC_ALL, rg, hd, s);

          rMultNumeratorAll += n_hv.mult_no_art(hd) * n_hv.adults(VAC_ALL, rg, hd, s);
          rMultDenominatorAll += n_hv.adults(VAC_ALL, rg, hd, s);

        }

        // on ART
        for (int hd = CD4_GT500_ART; hd <= CD4_LT50_ART; ++hd) {
          rMultNumerator += n_hv.mult_art(hd-hOnArt) * n_hv.adults(VAC_ALL, rg, hd, s);
          rMultDenominator += n_hv.adults(VAC_ALL, rg, hd, s);

          rMultNumeratorAll += n_hv.mult_art(hd-hOnArt) * n_hv.adults(VAC_ALL, rg, hd, s);
          rMultDenominatorAll += n_hv.adults(VAC_ALL, rg, hd, s);

        }

        i_hv.r_mult(rg, s) = (rMultDenominator > 0.0) ? vaccine_factor * (rMultNumerator / rMultDenominator) / rMultEqlb : 1.0;

      }  // r

    }  // s

    // both sexes
    i_hv.r_mult(RG_ALL, S_ALL) = (rMultDenominatorAll > 0.0) ? vaccine_factor * (rMultNumeratorAll / rMultDenominatorAll) / rMultEqlb : 1.0;

    // MSM
    real_type vaccine_factor_m = 1.0 - p_hv.rn_vac_params(VAC_EFF)
                                * (n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_MALE) - n_hv.adults(VAC_UNV, RG_ALL, CD4_ALL, S_MALE))
                                / n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_MALE);

    rMultNumeratorAll = 0.0;
    rMultDenominatorAll = 0.0;
    for (int rg = RG_MSM; rg <= RG_MSMIDU; ++rg) {
      // not on ART
      for (int hd = CD4_PRIM; hd <= CD4_LT50; ++hd) {
        rMultNumeratorAll += n_hv.mult_no_art(hd) * n_hv.adults(VAC_ALL, rg, hd, S_MALE);
        rMultDenominatorAll += n_hv.adults(VAC_ALL, rg, hd, S_MALE);
      }

      // on ART
      for (int hd = CD4_GT500_ART; hd <= CD4_LT50_ART; ++hd) {
        rMultNumeratorAll += n_hv.mult_art(hd-hOnArt) * n_hv.adults(VAC_ALL, rg, hd, S_MALE);
        rMultDenominatorAll += n_hv.adults(VAC_ALL, rg, hd, S_MALE);
      }

    }  // r

    for (int rg = RG_MSM; rg <= RG_MSMIDU; ++rg) {
      i_hv.r_mult(rg, S_MALE) = (rMultDenominatorAll > 0.0) ? vaccine_factor_m
                      * (rMultNumeratorAll / rMultDenominatorAll) / rMultEqlb : 0.0;
    }

  }

  void calc_prevalence(int t, int hiv_step) {
    real_type denom_s_both = 0.0;
    real_type plhiv_s_both = 0.0;

    real_type denom = 0.0;
    real_type plhiv = 0.0;

    auto& n_hv = state_next.hv;
    for (int rg = RG_NONE; rg <= RG_ALL; ++rg) {
      denom_s_both = 0.0;
      plhiv_s_both = 0.0;

      for (int s = S_MALE; s <= S_FEMALE; ++s) {
        denom = 0.0;
        plhiv = 0.0;
        for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
          for (int hd = CD4_NEG; hd <= CD4_LT50_ART; ++hd) {
            if (!((s == S_FEMALE) && (rg >= RG_MSM) && (rg <= RG_MSMIDU))) {
              denom += n_hv.adults(v, rg, hd, s);
              if (hd >= CD4_PRIM) {
                plhiv += n_hv.adults(v, rg, hd, s);
              }
            }

          }  // hd

        }  // v

        n_hv.prevalence(rg, s) = (denom > 0.0) ? plhiv / denom : 0.0;

        denom_s_both += denom;
        plhiv_s_both += plhiv;

      }  // s

      n_hv.prevalence(rg, S_ALL) = (denom_s_both > 0.0) ? plhiv_s_both / denom_s_both : 0.0;

    }  // rg
  }

  void calc_new_infections(int t, int hiv_step) {
    // from goals
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    real_type totalSexActs = 0.0;
    real_type totalSexActsF = 0.0;
    real_type totalSexActsM = 0.0;

    real_type PrevM = 0.0;
    real_type PrevF = 0.0;
    real_type PrevML = 0.0;
    real_type PrevFL = 0.0;
    real_type denom = 0.0;

    real_type rMultM = 1.0;
    real_type rMultF = 1.0;

    real_type foi = 0.0;

    // Sex‑acts ratio, toggled by balanceSexActs
    real_type SexActsRatioM = 1.0;
    real_type SexActsRatioF = 1.0;

    // for idu
    real_type PrevM_IDU = 0.0;
    real_type PrevF_IDU = 0.0;

    real_type rMultM_IDU = 1.0;
    real_type rMultF_IDU = 1.0;

    real_type SexActsRatioM_IDU = 1.0;
    real_type SexActsRatioF_IDU = 1.0;

    real_type circum = p_hv.rn_coverage(RN_MC15_49, t);
    real_type vmm_coverage = 0.0;

    real_type hiv_neg_pops[nRG][nNS] = {};

    real_type vacc_effect = 0.0;

    real_type hiv_neg_pop = 0.0;


    for (int rg = RG_LRH; rg <= RG_HRH; ++rg) {
      if (rg == RG_LRH) {
        totalSexActsF = n_hv.adults(VAC_ALL, RG_LRH, CD4_ALL, S_FEMALE)
            * i_hv.i_num_partners(RG_LRH + RG_NONE_F3)
            * p_hv.b_sex_acts(RG_LRH + RG_NONE_F3, t);
        PrevFL = n_hv.adults(VAC_ALL, RG_LRH, CD4_ALL, S_FEMALE)
            * n_hv.prevalence(RG_LRH, S_FEMALE);
        denom = n_hv.adults(VAC_ALL, RG_LRH, CD4_ALL, S_FEMALE);

        for (int rg2 = RG_MRH; rg2 <= RG_IDU; ++rg2) {
          totalSexActsF += n_hv.adults(VAC_ALL, rg2, CD4_ALL, S_FEMALE)
              * i_hv.i_num_partners(rg2 + RG_NONE_F3)
              * p_hv.b_sex_acts(rg2 + RG_NONE_F3, t)
              * p_hv.b_married_prop(rg2 + RG_NONE_F3);
          PrevFL += n_hv.adults(VAC_ALL, rg2, CD4_ALL, S_FEMALE)
              * n_hv.prevalence(rg2, S_FEMALE)
              * p_hv.b_married_prop(rg2 + RG_NONE_F3);
          denom += n_hv.adults(VAC_ALL, rg2, CD4_ALL, S_FEMALE)
              * p_hv.b_married_prop(rg2 + RG_NONE_F3);
        }

        PrevFL = (denom > 0.0) ? PrevFL / denom : 0.0;
        PrevF = PrevFL;

        totalSexActsM = n_hv.adults(VAC_ALL, RG_LRH, CD4_ALL, S_MALE)
            * i_hv.i_num_partners(rg) * p_hv.b_sex_acts(rg, t);
        PrevML = n_hv.adults(VAC_ALL, RG_LRH, CD4_ALL, S_MALE)
            * n_hv.prevalence(RG_LRH, S_MALE);
        denom = n_hv.adults(VAC_ALL, RG_LRH, CD4_ALL, S_MALE);
        for (int rg2 = RG_MRH; rg2 <= RG_MSMIDU; ++rg2) {
          totalSexActsM += n_hv.adults(VAC_ALL, rg2, CD4_ALL, S_MALE)
              * i_hv.i_num_partners(rg2) * p_hv.b_sex_acts(rg2, t)
              * p_hv.b_married_prop(rg2);
          PrevML += n_hv.adults(VAC_ALL, rg2, CD4_ALL, S_MALE)
              * n_hv.prevalence(rg2, S_MALE) * p_hv.b_married_prop(rg2);
          denom += n_hv.adults_ts(VAC_ALL, rg2, CD4_ALL, S_MALE)
              * p_hv.b_married_prop(rg2);
        }

        PrevML = (denom > 0.0) ? PrevML / denom : 0.0;
        PrevM = PrevML;
      } else {
        totalSexActsF = n_hv.adults(VAC_ALL, rg, CD4_ALL, S_FEMALE)
            * i_hv.i_num_partners(rg + RG_NONE_F3)
            * p_hv.b_sex_acts(rg + RG_NONE_F3, t);
        totalSexActsM = n_hv.adults(VAC_ALL, rg, CD4_ALL, S_MALE)
            * i_hv.i_num_partners(rg) * p_hv.b_sex_acts(rg, t);

        PrevM = std::max(n_hv.prevalence(rg, S_MALE), PrevML);
        PrevF = std::max(n_hv.prevalence(rg, S_FEMALE), PrevFL);
      }

      // Global multiplier
      rMultM = i_hv.r_mult(RG_ALL, S_ALL);
      rMultF = i_hv.r_mult(RG_ALL, S_ALL);

      // Assume equal adjustment for men and women
      totalSexActs = (totalSexActsM + totalSexActsF) / 2.0;
      SexActsRatioM = (totalSexActsM > 0.0) ? totalSexActs / totalSexActsM : 1.0;
      SexActsRatioF = (totalSexActsF > 0.0) ? totalSexActs / totalSexActsF : 1.0;

      // Toggle balancing
      if (p_hv.b_balance_sex_acts == 0) {
        SexActsRatioM = 1.0;
        SexActsRatioF = 1.0;
      }

      // Assuming IDU have medium‑risk sexual behaviour and are exposed to the
      // same risk as HV_MRH
      if (rg == RG_MRH) {
        PrevM_IDU = PrevM;
        PrevF_IDU = PrevF;

        rMultM_IDU = rMultM;
        rMultF_IDU = rMultF;

        SexActsRatioM_IDU = SexActsRatioM;
        SexActsRatioF_IDU = SexActsRatioF;
      }

      // men
      int s = S_MALE;
      for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {

        if( n_hv.adults(v, rg, CD4_NEG, s) <= 0.0 ){
          continue;
        }

        vacc_effect = 0.0;

        // no new infections in take category
        if (v == VAC_TAKE) {
          n_hv.new_inf_vrs(VAC_TAKE, rg, S_MALE) = 0.0;
          continue;
        }

        // no_protection category same as unvaccinated
        //partial protection:
        if (v == VAC_PARTIAL) {
          vacc_effect = p_hv.rn_vac_params(VAC_EFF);
        }

        foi = 1.0 - std::pow(PrevF
                          * std::pow(
                              (1.0 - p_hv.epi_transm_hiv_F * (1.0 - vacc_effect)
                                   * rMultF
                                   * ((1.0 - circum) + (1.0 - p_hv.epi_redwhen_circum(HV_SUCC)) * circum)
                                   * (1.0 + (p_hv.epi_transm_sti_mult - 1) * p_hv.epi_sti_prev(rg, t))
                                   * (1.0 - i_hv.i_condom_prop(rg) * p_hv.epi_condom_effect)
                                   * (1.0 - p_hv.prep_cov(S_MALE, rg, t) * i_hv.prep_effect(rg, s))
                                   * (1.0 - n_hv.cured_prop(rg, S_MALE))),
                              p_hv.b_sex_acts(rg, t) * SexActsRatioM
                          )
                      + (1.0 - PrevF),
                  i_hv.i_num_partners(rg)
            );

        hiv_neg_pop = n_hv.adults(v, rg, CD4_NEG, S_MALE);
        n_hv.new_inf_vrs(v, rg, S_MALE) = foi * hiv_neg_pop;

      }  // v, S_MALE

      // women
      s = S_FEMALE;
      for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {

        if( n_hv.adults(v, rg, CD4_NEG, s) <= 0.0){
          continue;
        }

        vmm_coverage = p_hv.rn_vmm_coverage_all(t);
        if (p_hv.rn_vmm_coverage_type != VMM_COV_ALLRISK) {
          vmm_coverage = p_hv.rn_vmm_coverage_rg(rg, t);
        }

        vacc_effect = 0.0;

        // no new infections in take category
        if (v == VAC_TAKE) {
          n_hv.new_inf_vrs(VAC_TAKE, rg, S_FEMALE) = 0.0;
          continue;
        }

        // no_protection category same as unvaccinated
        //partial protection:
        if (v == VAC_PARTIAL) {
          vacc_effect = p_hv.rn_vac_params(VAC_EFF);
        }

        foi = 1.0 - std::pow( PrevM
                        * std::pow(
                            (1.0 - p_hv.epi_transm_hiv_F * (1.0 - vacc_effect)
                                 * p_hv.epi_transm_mult_M * rMultM
                                 *  // CDP check per act prob
                                 ((1.0 - circum) + (1.0 - p_hv.epi_redwhen_circum(HV_INF)) * circum)
                                 * (1.0 + (p_hv.epi_transm_sti_mult - 1) * p_hv.epi_sti_prev(rg + RG_NONE_F3, t))
                                 *  // check index
                                 (1.0 - i_hv.i_condom_prop(rg) * p_hv.epi_condom_effect) *
                                 (1.0 - p_hv.prep_cov(S_FEMALE, rg, t) * i_hv.prep_effect(rg, s))
                                 * (1.0 - vmm_coverage * p_hv.rn_vmm_effect)
                                 * (1.0 - n_hv.cured_prop(rg, S_FEMALE))),
                            p_hv.b_sex_acts(rg, t) * SexActsRatioF
                        )
                    + (1.0 - PrevM),
                i_hv.i_num_partners(rg + RG_NONE_F3)
            );

        hiv_neg_pop = n_hv.adults(v, rg, CD4_NEG, S_FEMALE);
        n_hv.new_inf_vrs(v, rg, S_FEMALE) = foi * hiv_neg_pop;

      }  // v, S_FEMALE

    }  // rg

    for (int rg = RG_MSM; rg <= RG_MSM; ++rg) {
      // MSM multiplier calculated among MSM groups only
      rMultM = i_hv.r_mult(rg, S_MALE);

      // men
      int s = S_MALE;

      for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {

        if( n_hv.adults(v, rg, CD4_NEG, S_MALE) <= 0.0){
          continue;
        }

        vacc_effect = 0.0;

        // no new infections in take category
        if (v == VAC_TAKE) {
           n_hv.new_inf_vrs(VAC_TAKE, rg, S_MALE) = 0.0;
          continue;
        }

        // no_protection category same as unvaccinated
        //partial protection:
        if (v == VAC_PARTIAL) {
          vacc_effect = p_hv.rn_vac_params(VAC_EFF);
        }

        PrevM = std::max(n_hv.prevalence(rg, S_MALE), PrevML);
        foi = 1.0 - std::pow( PrevM
                          * std::pow(
                              (1.0 - p_hv.epi_transm_hiv_F * (1.0 - vacc_effect)
                                   * rMultM
                                   * ((1.0 - circum) + (1.0 - p_hv.epi_redwhen_circum(HV_INF)) * circum)
                                   * p_hv.epi_transm_mult_MSM
                                   * (1.0 + (p_hv.epi_transm_sti_mult - 1) * p_hv.epi_sti_prev(rg, t))
                                   * (1.0 - i_hv.i_condom_prop(rg) * p_hv.epi_condom_effect)
                                   * (1.0 - p_hv.prep_cov(S_MALE, rg, t) * i_hv.prep_effect(rg, s))
                                   * (1.0 - n_hv.cured_prop(rg, S_MALE))),
                              p_hv.b_sex_acts(rg, t)
                          )
                      + (1.0 - PrevM),
                  i_hv.i_num_partners(rg)
            );

        hiv_neg_pop = n_hv.adults(v, rg, CD4_NEG, S_MALE);
        n_hv.new_inf_vrs(v, rg, S_MALE) = std::max(0.0, foi * hiv_neg_pop);

      }  // v, S_MALE

    }  // rg MSM

    int rg = RG_IDU;
    // Assuming IDU have medium risk sexual behavior, see r=HV_MRH above
    PrevM = PrevM_IDU;
    PrevF = PrevF_IDU;
    SexActsRatioM = SexActsRatioM_IDU;
    SexActsRatioF = SexActsRatioF_IDU;

    rMultM = i_hv.r_mult(RG_ALL, S_MALE);
    rMultF = i_hv.r_mult(RG_ALL, S_FEMALE);

    // IDU risk, partner prevalence calc
    real_type PrevB = 0.0;
    if (n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_MALE)
            + n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_FEMALE)
            + n_hv.adults(VAC_ALL, RG_MSMIDU, CD4_ALL, S_MALE) > 0.0)
    {
      PrevB = (n_hv.prevalence(RG_IDU, S_MALE)
                   * n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_MALE)
               + n_hv.prevalence(RG_IDU, S_FEMALE)
                   * n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_FEMALE)
               + n_hv.prevalence(RG_MSMIDU, S_MALE)
                   * n_hv.adults(VAC_ALL, RG_MSMIDU, CD4_ALL, S_MALE))
          / (n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_MALE)
             + n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_FEMALE)
             + n_hv.adults(VAC_ALL, RG_MSMIDU, CD4_ALL, S_MALE));
    }

    if (i_hv.i_idu_share_prop > 0.0) {
      PrevB = PrevB / i_hv.i_idu_share_prop;
    }

    PrevB = std::max(
        PrevB,
        std::min(
            n_hv.prevalence(RG_LRH, S_MALE), n_hv.prevalence(RG_LRH, S_FEMALE)
        )
    );

    real_type SusceptibleIDU = 0.0;
    real_type foi_idu_sex = 0.0;
    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {

         if( n_hv.adults(v, RG_IDU, CD4_NEG, s) <= 0.0){
          continue;
        }

        SusceptibleIDU = n_hv.adults(v, RG_IDU, CD4_NEG, s);
        if (SusceptibleIDU
            > n_hv.adults(v, RG_IDU, CD4_ALL, s) * i_hv.i_idu_share_prop
                - (n_hv.adults(v, RG_IDU, CD4_ALL, s)
                   - n_hv.adults(v, RG_IDU, CD4_NEG, s)))
        {
          SusceptibleIDU =
              n_hv.adults(v, RG_IDU, CD4_ALL, s) * i_hv.i_idu_share_prop
              - (n_hv.adults(v, RG_IDU, CD4_ALL, s)
                 - n_hv.adults(v, RG_IDU, CD4_NEG, s));
        }

        vacc_effect = 0.0;

        // no new infections in take category
        if (v == VAC_TAKE) {
         n_hv.new_inf_vrs(VAC_TAKE, rg, s) = 0.0;
         continue;
        }

        // no_protection category same as unvaccinated
        //partial protection:
        if (v == VAC_PARTIAL) {
          vacc_effect = p_hv.rn_vac_params(VAC_EFF);
        }

        if (s == S_MALE) {
          foi_idu_sex = 1.0 - std::pow( PrevF
                          * std::pow(
                              (1.0 - p_hv.epi_transm_hiv_F * (1.0 - vacc_effect)
                                   * rMultF
                                   * ((1.0 - circum) + (1.0 - p_hv.epi_redwhen_circum(HV_SUCC)) * circum)
                                   * (1.0 + (p_hv.epi_transm_sti_mult - 1) * p_hv.epi_sti_prev(rg, t))
                                   * (1.0 - i_hv.i_condom_prop(rg) * p_hv.epi_condom_effect)
                                   * (1.0 - p_hv.prep_cov(S_MALE, rg, t) * i_hv.prep_effect(rg, s))
                                   * (1.0 - n_hv.cured_prop(rg, S_MALE))),
                              p_hv.b_sex_acts(rg, t) * SexActsRatioM
                          )
                      + (1.0 - PrevF),
                  i_hv.i_num_partners(rg)
              );

        } else {
          foi_idu_sex = 1.0 - std::pow( PrevM
                          * std::pow(
                              (1.0 - p_hv.epi_transm_hiv_F * p_hv.epi_transm_mult_M *  (1.0 - vacc_effect)
                                   * rMultM *  // CDP check per act prob
                                   ((1.0 - circum) + (1.0 - p_hv.epi_redwhen_circum(HV_INF)) * circum) *
                                   (1.0 + (p_hv.epi_transm_sti_mult - 1) * p_hv.epi_sti_prev(rg + RG_NONE_F3, t)) *
                                   (1.0 - i_hv.i_condom_prop(rg) * p_hv.epi_condom_effect) *
                                   (1.0 - p_hv.prep_cov(S_FEMALE, rg, t) * i_hv.prep_effect(rg, s)) *
                                   (1.0 - n_hv.cured_prop(rg, S_FEMALE))),
                              p_hv.b_sex_acts(rg, t) * SexActsRatioF
                          )
                      + (1.0 - PrevM),
                  i_hv.i_num_partners(rg + RG_NONE_F3)
              );
        }

        // needle sharing
        real_type idu_inf = 0.0;
        idu_inf = p_hv.b_foi_idu(s, t) * i_hv.r_mult(RG_ALL, S_ALL) * PrevB
            * SusceptibleIDU;
        n_hv.new_inf_vrs(v, rg, s) = std::max(idu_inf, 0.0);

        SusceptibleIDU = n_hv.adults(v, RG_IDU, CD4_NEG, s);
        // sexual transmission
        n_hv.new_inf_vrs(v, rg, s) += std::max(0.0, foi_idu_sex * SusceptibleIDU);

      }  // v

    }  // s

  }

  void calc_rescale_infections(int t) {
    const auto& p_hv = pars.hv;
    auto& n_hv = state_next.hv;

    real_type new_inf_goals = 0.0;
    real_type new_inf_lf = 0.0;
    real_type scale_factor = 1.0;

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      new_inf_goals = 0.0;
      new_inf_lf = 0.0;

      // get total new inf by sex
      for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
        for (int rg = RG_LRH; rg <= RG_MSMIDU; ++rg) {
          if (!((s == S_FEMALE) && (rg >= RG_MSM))) {
            // New inf from goals used as inputs
            new_inf_goals += p_hv.out_new_inf(s, rg, v, t);

            // New inf from goals used as inputs
            new_inf_lf += n_hv.new_inf_vrs(v, rg, s);
          }

        }  // rg

      }  // v

      scale_factor = 1.0;
      if (t < p_hv.goals_base_year_idx) {
        scale_factor = (new_inf_lf > 0) ? (new_inf_goals / new_inf_lf) : 1.0;
        n_hv.new_inf_scale_factor(s) = scale_factor;
      } else {
        scale_factor = n_hv.new_inf_scale_factor(s);
      }

      // rescale
      for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
        for (int rg = RG_LRH; rg <= RG_MSMIDU; ++rg) {
          if (!((s == S_FEMALE) && (rg >= RG_MSM))) {
            // rescale lf new inf to match goals at level of sex
            n_hv.new_inf_vrs(v, rg, s) =
                scale_factor * n_hv.new_inf_vrs(v, rg, s);
          }

        }  // rg

      }  // v

    }  // s
  }

  void add_new_infections_goals(int t, int hiv_step) {
    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;

    // new infections from goals
    const auto& p_hv = pars.hv;
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;

    nda::fill(
        n_hv.new_inf_s, 0.0
    );  // new infections for each t, by sex, for aim

    int a25 = 10;
    real_type new_inf_input = 0.0;

    for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
      for (int rg = RG_LRH; rg <= RG_MSMIDU; ++rg) {
        for (int s = S_MALE; s <= S_FEMALE; ++s) {
          if (!((s == S_FEMALE) && (rg >= RG_MSM))) {
            // New inf from goals used as inputs
            new_inf_input = p_hv.out_new_inf(s, rg, v, t);

            // Remove new infections from HIV‑negative category
            n_hv.adults(v, rg, CD4_NEG, s) -= opts.dt * new_inf_input;
            if (n_hv.adults(v, rg, CD4_NEG, s) < 0.0) {
              n_hv.adults(v, rg, CD4_NEG, s) = 0.0;
            }
            n_hv.new_inf_s(s) += opts.dt * new_inf_input;
            n_hv.total_new_infections += opts.dt * new_inf_input;

            // Add new infections to each HIV‑positive stage
            for (int hd = CD4_PRIM; hd <= CD4_LT50; ++hd) {
              int hd_hds = 0;  // set prim

              if (hd == CD4_GT500) {
                continue;  // see goals
              }

              if (hd >= CD4_350_500) {
                hd_hds = hd - CD4_GT500;  // CD4_350_500 and higher indices
              }

              n_hv.adults(v, rg, hd, s) += opts.dt * new_inf_input
                  * p_ha.cd4_initial_distribution(hd_hds, a25, s);
            }
          }
        }  // s
      }  // r
    }  // v

    real_type num_s_both = 0.0;
    real_type denom_s_both = 0.0;
    if (hiv_step == 0) {
      for (int s = S_MALE; s <= S_FEMALE; ++s) {
        real_type num = 1.0 / opts.dt * n_hv.new_inf_s(s);
        // dp needs annual new infections, distributes for each hiv_step
        real_type denom = i_hv.dp_aging_denom_1549(POP_H_HIVNeg, CD4_NEG, s);

        num_s_both += num;
        denom_s_both += denom;

        n_hv.incidence_goals(s) = num / denom;
        n_hv.new_infections_goals(s) = num;
      }

      n_hv.incidence_goals(S_ALL) = num_s_both / denom_s_both;
      n_hv.new_infections_goals(S_ALL) = num_s_both;
    }
  }

  void add_new_infections(int t, int hiv_step) {
    const auto& p_ha = pars.ha;
    auto& n_ha = state_next.ha;

    // new infections from goals
    const auto& p_hv = pars.hv;
    auto& n_hv = state_next.hv;
    auto& i_hv = intermediate.hv;

    nda::fill( n_hv.new_inf_s, 0.0 );  // new infections for each t, by sex, for aim

    int a25 = 10;
    real_type new_inf_input = 0.0;

    for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
      for (int rg = RG_LRH; rg <= RG_MSMIDU; ++rg) {
        for (int s = S_MALE; s <= S_FEMALE; ++s) {
          if (!((s == S_FEMALE) && (rg >= RG_MSM))) {
            // New inf from goals used as inputs
            new_inf_input = n_hv.new_inf_vrs(v, rg, s);

            // Remove new infections from HIV‑negative category
            n_hv.adults(v, rg, CD4_NEG, s) -= opts.dt * new_inf_input;
            if (n_hv.adults(v, rg, CD4_NEG, s) < 0.0) {
              n_hv.adults(v, rg, CD4_NEG, s) = 0.0;
            }
            n_hv.new_inf_s(s) += opts.dt * new_inf_input;
            n_hv.total_new_infections += opts.dt * new_inf_input;

            // Add new infections to each HIV‑positive stage
            for (int hd = CD4_PRIM; hd <= CD4_LT50; ++hd) {
              int hd_hds = 0;  // set prim

              if (hd == CD4_PRIM) {
                hd_hds = 0;
              }

              if (hd == CD4_GT500) {
                continue;  // see goals
              }

              if (hd >= CD4_350_500) {
                hd_hds = hd - CD4_GT500;  // CD4_350_500 and higher indices
              }

              n_hv.adults(v, rg, hd, s) += opts.dt * new_inf_input
                  * p_ha.cd4_initial_distribution(hd_hds, a25, s);
            }
          }
        }  // s
      }  // r
    }  // v

    real_type num_s_both = 0.0;
    real_type denom_s_both = 0.0;

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      real_type num = 1.0 / opts.dt * n_hv.new_inf_s(s);
      // dp needs annual new infections, distributes for each hiv_step
      real_type denom = i_hv.dp_aging_denom_1549(POP_H_HIVNeg, CD4_NEG, s);

      num_s_both += num;
      denom_s_both += denom;

      n_hv.incidence_goals(s) = num / denom;
      n_hv.new_infections_goals(s) += num;
    }

    n_hv.incidence_goals(S_ALL) = num_s_both / denom_s_both;
    n_hv.new_infections_goals(S_ALL) += num_s_both;

  }

  void allocate_art(int t) {
    // from aim
    auto& i_ha = intermediate.ha;
    const auto& p_ha = pars.ha;

    // from goals
    auto& n_hv = state_next.hv;
    const auto& c_hv = state_curr.hv;
    auto& i_hv = intermediate.hv;
    const auto& p_hv = pars.hv;

    real_type not_receiving_art_vrhs[nVAC][nRG][nCD4p][nNS] = {};
    real_type eligible_art_vrhs[nVAC][nRG][nCD4p][nNS] = {};

    real_type receiving_art_vrs[nVAC][nRG][nNS] = {};
    real_type not_receiving_art_vrs[nVAC][nRG][nNS] = {};
    real_type start_art[nVAC][nRG][nNS] = {};
    real_type sum_elig_art[nVAC][nRG][nNS] = {};

    real_type sex_age_hiv[POP_H_ALL][nNS] = {};
    real_type art_cov[RG_TOTAL_ART][nNS] = {};
    bool kp_cd4_elig[RG_TOTAL_ART][nNS] = {};

    real_type new_patients[nCD4p] = {};
    real_type mort_rate[nCD4p] = {};
    real_type prop1[nCD4p] = {};
    real_type prop2[nCD4p] = {};
    real_type prop_avg[nCD4p] = {};

    real_type prop_gt12m;
    real_type prop_lt12m;
    real_type num_ltfu;
    real_type prop_by_mort;
    real_type prop_by_elig;
    real_type remaining_new_art;
    real_type entrants;

    real_type ltfu;
    real_type new_art_cap;
    real_type sum1;

    const int FSW_ELIG=0;
    const int MSM_ELIG=0;
    const int IDU_ELIG=0;

    for (int rg = RG_NONE; rg <= RG_TOTAL1; ++rg) {
      for (int s = S_MALE; s <= S_FEMALE; ++s) {
        kp_cd4_elig[rg][s] = true;
      }
    }

    prop_by_mort = p_ha.initiation_mortality_weight;
    prop_by_elig = 1.0 - prop_by_mort;

    // do LTFU dynamics, before achieving a CD4 coverage
    // convert the LTFU % to a per-capita annual rate
    ltfu = -std::log(1.0 - p_hv.art_interrupt_rate(t));
    if(t > p_hv.goals_base_year_idx){
      ltfu = -std::log(1.0 - p_hv.art_interrupt_rate(t)*(1.0 - p_hv.long_act_treat_cov(t) * p_hv.long_act_treat_eff_ltfu));
    }

    new_art_cap = 0.99;

    const int hOnArt = 7;
    if (ltfu > 0) {
      for (int s = S_MALE; s <= S_FEMALE; ++s) {
        for (int hd = CD4_LT50; hd >= CD4_GT500; --hd) {
          prop_gt12m = i_hv.dp_aging_denom_1549(POP_H_ARTgt12m, hd, s);
          prop_lt12m = i_hv.dp_aging_denom_1549(POP_H_ARTlt6m, hd, s)
              + i_hv.dp_aging_denom_1549(POP_H_ART6to12m, hd, s);

          if ((prop_lt12m + prop_gt12m) > 0.0) {
            prop_gt12m = prop_gt12m / (prop_gt12m + prop_lt12m);
          } else {
            prop_gt12m = 0.0;
          }

          prop_lt12m = 1.0 - prop_gt12m;

          for (int rg = RG_NONE; rg <= RG_TOTAL1; ++rg) {
            for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
              num_ltfu = ltfu / opts.hts_per_year * n_hv.adults(v, rg, hd + hOnArt, s);

              if (hd == CD4_GT500) {
                n_hv.adults(v, rg, hd, s) += num_ltfu;
              } else {
                n_hv.adults(v, rg, hd - 1, s) += prop_gt12m * num_ltfu;
                n_hv.adults(v, rg, hd, s) += prop_lt12m * num_ltfu;
              }
              // note offset on h
              n_hv.adults(v, rg, hd + hOnArt, s) -= num_ltfu;
            }  // v
          }  // rg
        }  // hd
      }  // s
    }  // if (ltfu > 0)

    for (int s = S_MALE; s <= S_FEMALE; ++s) {
      sex_age_hiv[POP_H_NoART][s] = 0.0;
      sex_age_hiv[POP_H_OnART][s] = 0.0;
      for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
        sex_age_hiv[POP_H_NoART][s] +=
            i_hv.dp_aging_denom_1549(POP_H_NoART, hd, s);
        sex_age_hiv[POP_H_OnART][s] +=
            i_hv.dp_aging_denom_1549(POP_H_OnART, hd, s);
      }
    }

    // set rg eligibility and year
    if (p_hv.pop_elig_treat(FSW_ELIG) == 1) {
      if (p_hv.pop_elig_year(FSW_ELIG)-opts.proj_start_year+1 <= t) {
        kp_cd4_elig[RG_HRH][S_FEMALE] = false;
      }
    }

    if (p_hv.pop_elig_treat(MSM_ELIG) == 1) {
      if (p_hv.pop_elig_year(MSM_ELIG)-opts.proj_start_year+1 <= t) {
        kp_cd4_elig[RG_MSM][S_MALE] = false;
      }
    }

    if (p_hv.pop_elig_treat(IDU_ELIG) == 1) {
      if (p_hv.pop_elig_year(IDU_ELIG)-opts.proj_start_year+1 <= t) {
        kp_cd4_elig[RG_IDU][S_MALE] = false;
        kp_cd4_elig[RG_IDU][S_FEMALE] = false;
      }
    }

    // Set ART coverage for all risk groups to AIM 15 to 49 coverage
    // CDP confirm order of inner loop calcs between leapfrog dp and goals
    if (sex_age_hiv[POP_H_OnART][S_FEMALE] > 0.0) {
      for (int rg = RG_NONE; rg <= RG_TOTAL1; ++rg) {
        art_cov[rg][S_MALE] = 0.0 / 100.0;
        if (sex_age_hiv[POP_H_NoART][S_MALE] + sex_age_hiv[POP_H_OnART][S_MALE] > 0.0)
        {
          art_cov[rg][S_MALE] = sex_age_hiv[POP_H_OnART][S_MALE]
              / (sex_age_hiv[POP_H_NoART][S_MALE]
                 + sex_age_hiv[POP_H_OnART][S_MALE]);
        }

        if(rg<RG_MSM){
          art_cov[rg][S_FEMALE] = 0.0 / 100.0;
          if (sex_age_hiv[POP_H_NoART][S_FEMALE]
                  + sex_age_hiv[POP_H_OnART][S_FEMALE] > 0.0)
          {
            art_cov[rg][S_FEMALE] = sex_age_hiv[POP_H_OnART][S_FEMALE]
                / (sex_age_hiv[POP_H_NoART][S_FEMALE]
                  + sex_age_hiv[POP_H_OnART][S_FEMALE]);
          }
        }
      }
    }

    // KP coverage from AIM input editor.
    // Note this is not a third way of computing initiations: it overrides the
    // coverage of the key population risk groups and then still takes the
    // coverage branch below, so it behaves as ART_ENTRY_NUMBER_OR_PERCENT with
    // the KP groups' coverage read from the input rather than derived.
    if (p_ha.art_entry_option == SS::ART_ENTRY_PERCENT_BY_RISK_GROUP) {
      // Coverage for males, risk groups HV_IDU … HV_MSM
      for (int rg = RG_IDU; rg <= RG_MSM; ++rg) {
        art_cov[rg][S_MALE] = p_hv.art_coverage_rg(S_MALE, rg, t);
      }

      // Coverage for females, risk groups HV_HRH … HV_IDU
      for (int rg = RG_HRH; rg <= RG_IDU; ++rg) {
        art_cov[rg][S_FEMALE] = p_hv.art_coverage_rg(S_FEMALE, rg, t);
      }
    }

    // Calc new ART by sex and rg and allocate it
    for (int s = S_MALE; s <= S_FEMALE; s++) {
      // calc numbers eligible
      for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
        int hd_hds = hd - CD4_GT500;
        for (int rg = RG_NONE; rg <= RG_TOTAL1; ++rg) {
          if (s == S_FEMALE && rg >= RG_MSM) {
            continue;
          }

          for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
            // not receiving ART (by sex & rg)
            not_receiving_art_vrs[v][rg][s] += n_hv.adults(v, rg, hd, s);  // CDP check adults or adultsTS
            not_receiving_art_vrhs[v][rg][hd][s] += n_hv.adults(v, rg, hd, s);

            // receiving ART (by sex & rg)
            receiving_art_vrs[v][rg][s] += n_hv.adults(v, rg, hd + hOnArt, s);

            if (kp_cd4_elig[rg][s] == true) {
              if (hd_hds >= i_ha.everARTelig_idx)  // CDP check role of this condition
              {
                // eligible ART (by rg, CD4, sex)
                eligible_art_vrhs[v][rg][hd][s] = n_hv.adults(v, rg, hd, s);
                sum_elig_art[v][rg][s] += n_hv.adults(v, rg, hd, s);
              }
            } else {
              // eligible ART (by rg, CD4, sex) – default path
              eligible_art_vrhs[v][rg][hd][s] = n_hv.adults(v, rg, hd, s);
              sum_elig_art[v][rg][s] += n_hv.adults(v, rg, hd, s);
            }

          }  // v
        }  // rg
      }  // hd

      for (int rg = RG_NONE; rg <= RG_TOTAL1; ++rg) {
        if (s == S_FEMALE && rg >= RG_MSM) {
          continue;
        }

        for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
          if (pars.ha.art_entry_option == SS::ART_ENTRY_INITIATION_RATE) {
            // Initiations are a rate applied to the treatment gap: PLHIV, plus
            // this year's new infections, minus those already on ART.
            const real_type new_infections = c_hv.new_inf_vrs(v, rg, s);
            real_type plhiv = 0.0;
            for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
              plhiv += c_hv.adults(v, rg, hd, s);
            }

            const real_type treatment_gap = new_infections + plhiv;
            start_art[v][rg][s] = std::max(
                opts.dt * pars.ha.art_initiation_rate(s, t) * treatment_gap, 0.0);
          } else {
            // Coverage target: initiate however many are needed to reach the
            // input number/percent on ART.
            start_art[v][rg][s] =
                (not_receiving_art_vrs[v][rg][s] + receiving_art_vrs[v][rg][s])
                    * art_cov[rg][s] - receiving_art_vrs[v][rg][s];
          }

          // allocate new ART according to eligibility (Prop1) and mortality
          // (Prop2)
          for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
            prop1[hd] = 0.0;
            mort_rate[hd] = 0.0;
            prop2[hd] = 0.0;
            new_patients[hd] = 0.0;
          }

          if (sum_elig_art[v][rg][s] > 0.0) {
            for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
              if (hd == CD4_GT500) {
                prop1[hd] = (eligible_art_vrhs[v][rg][hd][s] +
                              eligible_art_vrhs[v][rg][CD4_PRIM][s]) / sum_elig_art[v][rg][s];
              } else {
                prop1[hd] = eligible_art_vrhs[v][rg][hd][s] / sum_elig_art[v][rg][s];
              }
            }  // hd
          }  // if

          // proportion according to mortality
          real_type sum1 = 0.0;
          for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
            sum1 += opts.dt * i_hv.hiv_mu(hd, s) * eligible_art_vrhs[v][rg][hd][s];
          }  // hd

          if (sum1 > 0.0) {
            for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
              mort_rate[hd] = opts.dt * i_hv.hiv_mu(hd, s) *
                              eligible_art_vrhs[v][rg][hd][s] / sum1;
            }  // hd
          }  // if

          // weighted average of number eligible and mortality
          remaining_new_art = start_art[v][rg][s];
          for (int hd = CD4_LT50; hd >= CD4_GT500; --hd) {
            sum1 = 0.0;
            for (int h2 = hd; h2 >= CD4_GT500; --h2) {
              sum1 += eligible_art_vrhs[v][rg][h2][s] * mort_rate[h2];
            }

            new_patients[hd] = (sum1 > 0.0) ? remaining_new_art
                    * (eligible_art_vrhs[v][rg][hd][s] * mort_rate[hd]) / sum1 : 0.0;

            // do not allocate more than are eligible in this CD4 band
            if (new_patients[hd] > eligible_art_vrhs[v][rg][hd][s]) {
              new_patients[hd] = eligible_art_vrhs[v][rg][hd][s];
            }

            // Reduce the pool of ART that still needs to be allocated
            remaining_new_art -= new_patients[hd];
            if (remaining_new_art < 0.0) {
              break;  // hd loop: no more ART to distribute
            }

          }  // hd

          // calculate proportion starting (Prop2)
          for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
            prop2[hd] = 0.0;
            if (start_art[v][rg][s] > 0.0) {
              prop2[hd] = new_patients[hd] / start_art[v][rg][s];
            }
          }  // hd

          // final average of the two methods (propAvg)
          for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
            prop_avg[hd] = prop_by_elig * prop1[hd] + prop_by_mort * prop2[hd];
          }  // hd

          // allocate the new ART by CD4 category
          if (start_art[v][rg][s] > 0.0) {
            for (int hd = CD4_GT500; hd <= CD4_LT50; ++hd) {
              entrants = prop_avg[hd] * start_art[v][rg][s];

              // cannot remove ART patients!
              entrants = std::max(entrants, 0.0);

              // respect the cap for new ART
              entrants = std::min(entrants, new_art_cap * n_hv.adults(v, rg, hd, s));

              if (n_hv.adults(v, rg, hd, s) - entrants > 0) {
                // HIV+ not on ART -> subtract entrants
                n_hv.adults(v, rg, hd, s) -= entrants;

                // HIV+ on ART -> add entrants (note the +10 offset)
                n_hv.adults(v, rg, hd + hOnArt, s) += entrants;
              }

              // keep track of newly started ART
              n_hv.newly_on_art(hd, s) += entrants;  // CDP check init of this array
            }  // hd
          }

        }  // v

      }  // end r

    }  // s
  };

  // post inner loop functions
  void sum_adult_pop_dims(int t) {
    auto& n_hv = state_next.hv;

    real_type sum = 0.0;

    for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
      for (int rg = RG_NONE; rg <= RG_MSMIDU; ++rg) {
        for (int hd = CD4_NEG; hd <= CD4_LT50_ART; ++hd) {
          n_hv.adults(v, rg, hd, S_ALL) = 0.0;
          sum = 0.0;
          for (int s = S_MALE; s <= S_FEMALE; ++s) {
            // Exclude female MSM and higher risk groups
            if (!((s == S_FEMALE) && (rg >= RG_MSM) && (rg <= RG_MSMIDU))) {
              sum += n_hv.adults(v, rg, hd, s);
            }
          }  // s

          n_hv.adults(v, rg, hd, S_ALL) = sum;

        }  // hd
      }  // rg
    }  // v

    for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
      for (int rg = RG_NONE; rg <= RG_MSMIDU; ++rg) {
        for (int s = S_MALE; s <= S_ALL; ++s) {
          n_hv.adults(v, rg, CD4_ALL, s) = 0.0;
          sum = 0.0;

          for (int hd = CD4_NEG; hd <= CD4_LT50_ART; ++hd) {
            // Exclude female MSM and higher risk groups
            if (!((s == S_FEMALE) && (rg >= RG_MSM))) {
              sum += n_hv.adults(v, rg, hd, s);
            }
          }  // hd
          n_hv.adults(v, rg, CD4_ALL, s) = sum;
        }  // s
      }  // rg
    }  // v

    for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
      for (int hd = CD4_NEG; hd <= CD4_ALL; ++hd) {
        for (int s = S_MALE; s <= S_ALL; ++s) {
          n_hv.adults(v, RG_ALL, hd, s) = 0.0;
          sum = 0.0;

          for (int rg = RG_NONE; rg <= RG_MSMIDU; ++rg) {
            // Exclude female MSM and higher risk groups
            if (!((s == S_FEMALE) && (rg >= RG_MSM))) {
              sum += n_hv.adults(v, rg, hd, s);
            }
          }  // hd

          n_hv.adults(v, RG_ALL, hd, s) = sum;

        }  // v
      }  // rg
    }  // s

    for (int rg = RG_NONE; rg <= RG_ALL; ++rg) {
      for (int hd = CD4_NEG; hd <= CD4_ALL; ++hd) {
        for (int s = S_MALE; s <= S_ALL; ++s) {
          n_hv.adults(VAC_ALL, rg, hd, s) = 0.0;
          sum = 0.0;

          for (int v = VAC_UNV; v <= VAC_NO_PROT; ++v) {
            // Exclude female MSM and higher risk groups
            if (!((s == S_FEMALE) && (rg >= RG_MSM) && (rg < RG_ALL))) {
              sum += n_hv.adults(v, rg, hd, s);
            }
          }  // v

          n_hv.adults(VAC_ALL, rg, hd, s) = sum;

        }  // s
      }  // hd
    }  // rg
  }

// private methods that we don't want people to call
private:
  void calc_adj_matrix(int t) {
    const auto& c_dp = state_curr.dp;

    const auto& p_hv = pars.hv;
    auto& i_hv = intermediate.hv;

    int GoalsBaseYearIdx = p_hv.goals_base_year_idx;
    nda::fill(i_hv.adj_coverage_base_yr, 0.0);
    nda::fill(i_hv.adj_coverage, 0.0);

    real_type popRatio = 0.0;
    real_type total_pop = 0.0;

    // Calc adjusted coverage
    for (int i = 1; i <= RN_MAX_BEHAV_INTVN; ++i) {
      i_hv.adj_coverage_base_yr(i) = p_hv.rn_coverage(i, GoalsBaseYearIdx);
    }

    for (int i = RN_COM_MOB; i <= RN_MAX_BEHAV_INTVN; ++i) {
      switch (i) {
        case RN_COMP_SEX_EDUC: {
          // Adjust coverage of school-based intervention for proportion of
          // adults that are in school
          real_type coverageDiff =
              p_hv.rn_coverage(i, t) - i_hv.adj_coverage_base_yr(i);
          real_type avgSchoolPop = 1.0 / 100.0
              * (p_hv.rn_pop_sizes(RN_POP_SEC_SCHOOL_MALE, t)
                 + p_hv.rn_pop_sizes(RN_POP_SEC_SCHOOL_FEMALE, t))
              / 2.0;

          popRatio = 0.0;
          total_pop = 0.0;
          for (int a = 10; a <= 49; ++a) {
            total_pop +=
                (c_dp.p_totpop(a, S_MALE) + c_dp.p_totpop(a, S_FEMALE));
            if (a <= 19) {
              popRatio +=
                  (c_dp.p_totpop(a, S_MALE) + c_dp.p_totpop(a, S_FEMALE));
            }
          }

          popRatio = (total_pop > 0.0) ? (popRatio / total_pop) : 0.0;

          i_hv.adj_coverage(i) = coverageDiff * avgSchoolPop * popRatio;
          break;
        }
        case RN_ECON_STRENGTH: {
          // Adjust coverage of workplace interventions for proportion of adults
          // in formal sector labor force
          real_type coverageDiff =
              p_hv.rn_coverage(i, t) - i_hv.adj_coverage_base_yr(i);

          popRatio = 0.0;
          total_pop = 0.0;
          for (int a = 10; a <= 49; ++a) {
            total_pop += (c_dp.p_totpop(a, S_FEMALE));
            if (a <= 19) {
              popRatio += (c_dp.p_totpop(a, S_FEMALE));
            }
          }

          popRatio = (total_pop > 0.0) ? (popRatio / total_pop) : 0.0;

          i_hv.adj_coverage(i) = coverageDiff * popRatio;
          break;
        }

        default: {
          i_hv.adj_coverage(i) =
              p_hv.rn_coverage(i, t) - i_hv.adj_coverage_base_yr(i);
          break;
        }
      }  // switch i
    }  // i
  }

  void calc_behav_matrix_impacts(int t) {
    const auto& p_hv = pars.hv;
    auto& i_hv = intermediate.hv;

    nda::fill(i_hv.adj_coverage_prod, 1.0);

    std::list<int> RN_UsedInterventions = {
        RN_COMP_SEX_EDUC,
        RN_FSW_OUTREACH,
        RN_ECON_STRENGTH,
        RN_IDU_HARM_RED,
        RN_IDU_NSEP,
        RN_IDU_DRUG_SUB,
        RN_MSM_OUTREACH,
        RN_CONDOMS,
        RN_CONDOM_SUPPLY,
        RN_ANC_TESTING
    };

    real_type value = 0.0;
    real_type impact = 1.0;

    for (int j = 1; j <= HV_MAX_BEHAV_IMPACTS; ++j) {
      for (int i = 1; i <= RN_MAX_BEHAV_INTVN; ++i) {
        // a few interventions are now excluded from the editors and RN calcs
        if (std::ranges::find(RN_UsedInterventions, i)
            == RN_UsedInterventions.end())
        {
          continue;
        }

        i_hv.adj_coverage_prod(j) *=
            (1.0 + i_hv.adj_coverage(i) * p_hv.hv_impact_matrix(i, j));
      }  // i

      i_hv.adj_coverage_prod(j) = std::max(i_hv.adj_coverage_prod(j), 0.0);

    }  // j

    for (int j = 1; j <= HV_MAX_BEHAV_IMPACTS; ++j) {
      switch (j) {
        // condom use
        case HV_IM_CONDOMS_LOW: {
          value = 1.0 - (1.0 - i_hv.i_condom_prop(RG_LRH))
                  * i_hv.adj_coverage_prod(HV_IM_CONDOMS_LOW);
          value = std::clamp(value, 0.0, 1.0);
          i_hv.i_condom_prop(RG_LRH) = value;

          break;
        }
        case HV_IM_CONDOMS_MED: {
          value = 1.0 - (1.0 - i_hv.i_condom_prop(RG_MRH))
                  * i_hv.adj_coverage_prod(HV_IM_CONDOMS_MED);
          value = std::clamp(value, 0.0, 1.0);
          i_hv.i_condom_prop(RG_MRH) = value;

          break;
        }

        case HV_IM_CONDOMS_HIGH: {
          value = 1.0 - (1.0 - i_hv.i_condom_prop(RG_HRH))
                  * i_hv.adj_coverage_prod(HV_IM_CONDOMS_HIGH);
          value = std::clamp(value, 0.0, 1.0);
          i_hv.i_condom_prop(RG_HRH) = value;

          break;
        }

        case HV_IM_CONDOMS_IDU: {
          value = 1.0 - (1.0 - i_hv.i_condom_prop(RG_IDU))
                  * i_hv.adj_coverage_prod(HV_IM_CONDOMS_IDU);
          value = std::clamp(value, 0.0, 1.0);
          i_hv.i_condom_prop(RG_IDU) = value;

          break;
        }

        case HV_IM_CONDOMS_MSM: {
          value = 1.0 - (1.0 - i_hv.i_condom_prop(RG_MSM))
                  * i_hv.adj_coverage_prod(HV_IM_CONDOMS_MSM);
          value = std::clamp(value, 0.0, 1.0);
          i_hv.i_condom_prop(RG_MSM) = value;

          break;
        }

        // number of partners
        case HV_IM_PARTNERS_LOW: {
          value = i_hv.i_num_partners(RG_LRH)
              * i_hv.adj_coverage_prod(HV_IM_PARTNERS_LOW);
          value = std::clamp(value, 0.0, 1000.0);
          i_hv.i_num_partners(RG_LRH) = value;

          break;
        }

        case HV_IM_PARTNERS_MED: {
          value = i_hv.i_num_partners(RG_MRH)
              * i_hv.adj_coverage_prod(HV_IM_PARTNERS_MED);
          value = std::clamp(value, 0.0, 1000.0);
          i_hv.i_num_partners(RG_MRH) = value;

          break;
        }

        case HV_IM_PARTNERS_HIGH: {
          value = i_hv.i_num_partners(RG_HRH)
              * i_hv.adj_coverage_prod(HV_IM_PARTNERS_HIGH);
          value = std::clamp(value, 0.0, 1000.0);
          i_hv.i_num_partners(RG_HRH) = value;

          break;
        }

        case HV_IM_PARTNERS_IDU: {
          value = i_hv.i_num_partners(RG_IDU)
              * i_hv.adj_coverage_prod(HV_IM_PARTNERS_IDU);
          value = std::clamp(value, 0.0, 1000.0);
          i_hv.i_num_partners(RG_IDU) = value;

          break;
        }

        case HV_IM_PARTNERS_MSM: {
          value = i_hv.i_num_partners(RG_MSM)
              * i_hv.adj_coverage_prod(HV_IM_PARTNERS_MSM);
          value = std::clamp(value, 0.0, 1000.0);
          i_hv.i_num_partners(RG_MSM) = value;

          break;
        }

        // age at first sex
        case HV_IM_AGE_FIRST_SEX: {
          impact = i_hv.adj_coverage_prod(HV_IM_AGE_FIRST_SEX);
          impact = std::max(impact, 0.0);
          i_hv.i_age_first_sex(S_MALE) *= impact;
          i_hv.i_age_first_sex(S_FEMALE) *= impact;

          break;
        }

        // IDU
        case HV_IM_UNSAFE_INJECT_IDU: {
          impact = i_hv.adj_coverage_prod(HV_IM_UNSAFE_INJECT_IDU);
          impact *= (i_hv.adj_coverage_prod(HV_IM_NEEDLE_SHARING_IDU));
          impact = std::max(impact, 0.0);
          i_hv.i_idu_share_prop *= impact;

          break;
        }

        default:
          break;

      }  // switch j

    }  // j
  }

  void calc_resource_needs() {
    const auto& c_dp = state_curr.dp;
    auto& n_dp = state_next.dp;

    auto& n_ha = state_next.ha;

    const auto& p_hc = pars.hc;
    auto& i_hc = intermediate.hc;

    const auto& p_hv = pars.hv;
    auto& i_hv = intermediate.hv;
    auto& n_hv = state_next.hv;

    std::list<int> RN_UsedInterventions = {
        RN_COMP_SEX_EDUC,    RN_FSW_OUTREACH,     RN_ECON_STRENGTH,
        RN_IDU_HARM_RED,     RN_IDU_NSEP,         RN_IDU_DRUG_SUB,
        RN_MSM_OUTREACH,     RN_CONDOMS,          RN_CONDOM_SUPPLY,
        RN_ANC_TESTING,      RN_MC15_49,          RN_PrEP_OralDaily,
        RN_PrEP_OralMonthly, RN_PrEP_OralPlusCon, RN_PrEP_Inject1Mo,
        RN_PrEP_Inject2Mo,   RN_PrEP_Inject6Mo,   RN_PrEP_Ring,
        RN_PrEP_bNABs,       RN_PrEP_Implant,     RN_PrEP_PEP,
        RN_HIV_TEST_ANC,     RN_HIV_WOMAN_COUNS,  RN_EXPOSED_INFANT_Dx,
        RN_ADULT_ON_ART,     RN_CHILD_ON_ART,     RN_VACCINES,
        RN_CURE_Adults,      RN_CURE_NEO,         RN_AHD_TX,
        RN_POC_CD4_INT,      RN_POC_VL_INT,       RN_VMM
    };

    real_type pop_reached = 0.0;
    real_type total_direct_costs = 0.0;

    real_type vmm_coverage = 0.0;
    real_type hahc_total_plhiv = i_hv.pop_hivpos_adults +
                                 i_hv.pop_hivpos_children +
                                 i_hv.pop_art_adults +
                                 i_hv.pop_art_children;

    real_type hahc_total_art = i_hv.pop_art_adults +
                               i_hv.pop_art_children;

    real_type hahc_total_plhiv_ahd = i_hv.pop_hivpos_adults_ahd +
                                     i_hv.pop_hivpos_children_ahd +
                                     i_hv.pop_art_adults_ahd +
                                     i_hv.pop_art_children_ahd;

    real_type hv_plhiv =   n_hv.adults(VAC_ALL, RG_ALL, CD4_ALL, S_ALL) -
                           n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_ALL);

    real_type plhiv_rg = 0.0;
    int nr =0;

    for (int i = 1; i <= RN_MAX_INTERVN; ++i) {
      // a few interventions are now excluded from the editors and RN calcs
      if (std::ranges::find(RN_UsedInterventions, i)
          == RN_UsedInterventions.end())
      {
        continue;
      }

      pop_reached = 0.0;

      // ------------------------------------------------------------
      switch (i) {
        case RN_COM_MOB:  // Community mobilization
        {
          pop_reached =
              (i_hv.dp_totpop_1549(S_MALE) + i_hv.dp_totpop_1549(S_FEMALE))
              * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_MASS_MEDIA:  // Mass media
        {
          pop_reached =
              (i_hv.dp_totpop_1549(S_MALE) + i_hv.dp_totpop_1549(S_FEMALE))
              * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_HTS:  // C&T (HTS)
        {
          pop_reached =
              (i_hv.dp_totpop_1549(S_MALE) + i_hv.dp_totpop_1549(S_FEMALE))
              * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_CONDOMS:  // condoms
        case RN_CONDOM_SUPPLY:  // condom supply
        {
          pop_reached +=  // i_hv.dp_totpop_1549(S_MALE)*
              (  // p_hv.b_behav_properties(RG_LRH, PERC_POP) *
                  n_hv.adults(VAC_ALL, RG_LRH, CD4_ALL, S_MALE)
                      * i_hv.i_num_partners(RG_LRH) * p_hv.b_sex_acts(RG_LRH, t)
                  +

                  // p_hv.b_behav_properties(RG_MRH, PERC_POP) *
                  n_hv.adults(VAC_ALL, RG_MRH, CD4_ALL, S_MALE)
                      * i_hv.i_num_partners(RG_MRH) * p_hv.b_sex_acts(RG_MRH, t)
                  +

                  // p_hv.b_behav_properties(RG_HRH, PERC_POP) *
                  n_hv.adults(VAC_ALL, RG_HRH, CD4_ALL, S_MALE)
                      * i_hv.i_num_partners(RG_HRH) * p_hv.b_sex_acts(RG_HRH, t)
                  +

                  n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_MALE)
                      * i_hv.i_num_partners(RG_IDU) * p_hv.b_sex_acts(RG_IDU, t)
                  +

                  n_hv.adults(VAC_ALL, RG_MSM, CD4_ALL, S_MALE)
                      * i_hv.i_num_partners(RG_MSM) * p_hv.b_sex_acts(RG_MSM, t)
              )
              * (1.0 + p_hv.rn_pop_sizes(RN_POP_CONDOM_WASTAGE, t) / 100.0);
          break;
        }

        case RN_COMP_SEX_EDUC:  // Comprehensive sexuality education
        {
          pop_reached = 0.0;
          for (int a = 10; a < 19; ++a) {
            pop_reached +=
                (c_dp.p_totpop(a, S_MALE)
                     * p_hv.rn_pop_sizes(RN_POP_SEC_SCHOOL_MALE, t)
                 + c_dp.p_totpop(a, S_FEMALE)
                     * p_hv.rn_pop_sizes(RN_POP_SEC_SCHOOL_FEMALE, t));
          }

          pop_reached *= 0.6 * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_OUT_OF_SCHOOL:  // Out of school youthl
        {
          pop_reached = 0.0;
          for (int a = 10; a < 19; ++a) {
            pop_reached +=
                (c_dp.p_totpop(a, S_MALE) + c_dp.p_totpop(a, S_FEMALE));
          }

          pop_reached *= 0.6 * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_ECON_STRENGTH:  // economic strengthening
        {
          pop_reached = 0.0;
          for (int a = 15; a < 24; ++a) {
            pop_reached += c_dp.p_totpop(a, S_FEMALE);
          }

          pop_reached *= p_hv.rn_coverage(i, t);
          break;
        }

        case RN_FSW_OUTREACH:  // Community mobilization
        {
          pop_reached = n_hv.adults(VAC_ALL, RG_HRH, CD4_ALL, S_FEMALE)
              * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_MSM_OUTREACH:  // MSM
        case RN_MSM_LUB: {
          pop_reached = (n_hv.adults(VAC_ALL, RG_MSM, CD4_ALL, S_MALE)
                         + n_hv.adults(VAC_ALL, RG_MSMLR, CD4_ALL, S_MALE)
                         + n_hv.adults(VAC_ALL, RG_MSMMR, CD4_ALL, S_MALE)
                         + n_hv.adults(VAC_ALL, RG_MSMHR, CD4_ALL, S_MALE)
                         + n_hv.adults(VAC_ALL, RG_MSMIDU, CD4_ALL, S_MALE))
              * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_IDU_HARM_RED:  // IDU
        case RN_IDU_CandT:
        case RN_IDU_OUTREACH: {
          pop_reached = (n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_MALE)
                         + n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_FEMALE))
              * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_IDU_NSEP: {
          pop_reached = (n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_MALE)
                         + n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_FEMALE))
              * p_hv.rn_pop_sizes(RN_POP_NUM_INJECT_YEAR, t) / 100.0
              *  // CDP check /100
              p_hv.rn_coverage(i, t);
          break;
        }

        case RN_IDU_DRUG_SUB: {
          pop_reached = (n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_MALE)
                         + n_hv.adults(VAC_ALL, RG_IDU, CD4_ALL, S_FEMALE))
              * p_hv.rn_pop_sizes(RN_POP_IDU_OPIOD_DEP, t) / 100.0
              *  // CDP check /100
              p_hv.rn_coverage(i, t);
          break;
        }

        case RN_ADULT_ON_ART:  // adult ART
        {
          pop_reached = i_hv.pop_art_adults;
          break;
        }

        case RN_CHILD_ON_ART:  // children ART
        {
          pop_reached = i_hv.pop_art_children;
          break;
        }

        case RN_HIV_TEST_ANC:  // HIV testing for all women attending ANC
        {
          pop_reached = (i_hc.need_PMTCT > 0.0)
              ? n_dp.births * (i_hc.on_PMTCT / i_hc.need_PMTCT)
              : 0.0;
          break;
        }

        case RN_HIV_WOMAN_COUNS:  // Counseling costs for HIV+ women
        {
          pop_reached = i_hc.on_PMTCT;
          break;
        }

        case RN_EXPOSED_INFANT_Dx:  // early infant diagnosis
        {
          pop_reached = i_hc.on_PMTCT;
          break;
        }

        case RN_ANC_TESTING:  // testing at ANC
        {
          pop_reached = n_dp.births * p_hv.rn_coverage(i, t);
          break;
        }

        case RN_MC15_49:  // adult VMMC
        {
          pop_reached = calc_vmmc_percent_to_number(t);
          break;
        }

        case RN_PrEP_OralDaily:  // PrEP
        case RN_PrEP_OralMonthly:
        case RN_PrEP_OralPlusCon:
        case RN_PrEP_Inject1Mo:
        case RN_PrEP_Inject2Mo:
        case RN_PrEP_Inject6Mo:
        case RN_PrEP_Ring:
        case RN_PrEP_bNABs:
        case RN_PrEP_Implant:
        case RN_PrEP_PEP: {
          int m = i - RN_PrEP_OralDaily;
          // prep for men
          for (int rg = RG_LRH; rg <= RG_MSMIDU; ++rg) {
            pop_reached += n_hv.adults(VAC_ALL, rg, CD4_NEG, S_MALE)
                * p_hv.prep_method_mix(S_MALE, rg, m, t)
                * p_hv.prep_cov(S_MALE, rg, t);
          }

          // prep for women
          for (int rg = RG_LRH; rg <= RG_IDU; ++rg) {
            pop_reached += n_hv.adults(VAC_ALL, rg, CD4_NEG, S_FEMALE)
                * p_hv.prep_method_mix(S_FEMALE, rg, m, t)
                * p_hv.prep_cov(S_FEMALE, rg, t);
          }

          break;
        }

        case RN_AHD_TX:  // treatment advanced HIV
        {
          pop_reached = p_hv.rn_ahd_treat_cov(t) * hahc_total_plhiv_ahd;
          break;
        }

        case RN_POC_CD4_INT:  // POC testing CD4
        {

          pop_reached = p_hv.rn_poc_cov(POC_CD4, t) * hahc_total_plhiv;
          break;
        }

        case RN_POC_VL_INT:  // POC testing VL
        {
          pop_reached = p_hv.rn_poc_cov(POC_VL, t) * hahc_total_art;

          break;
        }

        case RN_VACCINES:  // new vaccines
        {
          pop_reached = n_hv.total_new_vaccinations;
          break;
        }

        case RN_CURE_Adults:  // HIV cure
        {
          if (p_hv.rn_cure_coverage_type == CURE_COV_ALLRISK) {
            pop_reached = p_hv.rn_cure_coverage_all(t) * hahc_total_plhiv;

          } else {

            //males, cov weighted by RG size
            for (int rg = RG_LRH; rg <= RG_MSMIDU; ++rg) {
              plhiv_rg =  n_hv.adults(VAC_ALL, rg, CD4_ALL, S_MALE) -
                          n_hv.adults(VAC_ALL, rg, CD4_NEG, S_MALE);

              pop_reached +=  (hv_plhiv > 0.0) ? (plhiv_rg/hv_plhiv) : 0.0 *
                               p_hv.rn_cure_coverage_rg(rg, t) *
                               hahc_total_plhiv;
            }

            //females, cov weighted by RG size
            for (int rg = RG_LRH; rg <= RG_IDU; ++rg) {
              nr = rg + RG_NONE_F3;
              plhiv_rg =  n_hv.adults(VAC_ALL, rg, CD4_ALL, S_FEMALE) -
                          n_hv.adults(VAC_ALL, rg, CD4_NEG, S_FEMALE);

              pop_reached +=  (hv_plhiv > 0.0) ? (plhiv_rg/hv_plhiv) : 0.0 *
                               p_hv.rn_cure_coverage_rg(nr, t) *
                               hahc_total_plhiv;
            }
          }

          break;
        }

        case RN_CURE_NEO:  // HIV cure
        {
          // adjust hiv births to get those wh could be reached
          pop_reached = n_ha.hiv_births / (1.0 - p_hv.rn_cure_coverage_neonates(t) * p_hv.rn_cure_effect_neonates);
          pop_reached *= p_hv.rn_cure_coverage_neonates(t);

          break;
        }

        case RN_VMM:  // microbiome
        {
           //VMM_COV_ALLRISK
           vmm_coverage = p_hv.rn_vmm_coverage_all(t);
           pop_reached += vmm_coverage * n_hv.adults(VAC_ALL, RG_ALL, CD4_NEG, S_FEMALE);

          //VMM by RG
          if (p_hv.rn_vmm_coverage_type != VMM_COV_ALLRISK) {
           for (int rg = RG_NONE; rg <= RG_HRH; ++rg) {

            vmm_coverage = p_hv.rn_vmm_coverage_rg(rg, t);
            pop_reached += vmm_coverage * n_hv.adults(VAC_ALL, rg, CD4_NEG, S_FEMALE);

           }
          }

          break;
        }

        default:
          // No action for other interventions in this translation.
          break;
      }  // switch i

      n_hv.num_people_reached(i) = pop_reached;
      n_hv.resources_required(i) = pop_reached * p_hv.rn_unit_costs(i, t);
      total_direct_costs += n_hv.resources_required(i);

    }  // for i

    n_hv.resources_required(RN_DIRECT_COSTS) = total_direct_costs;
    n_hv.resources_required(RN_PROGRAM_COSTS) =
        total_direct_costs * (1.0 +  p_hv.program_support_markup(t));
    n_hv.resources_required(RN_TOTAL_COSTS) =
        n_hv.resources_required(RN_DIRECT_COSTS)
        + n_hv.resources_required(RN_PROGRAM_COSTS);

  }  // calc_resource_needs

  real_type calc_vmmc_percent_to_number(int t) {
    const auto& c_dp = state_curr.dp;
    const auto& p_hv = pars.hv;

    real_type popCurrent = 0.0;
    real_type popLastYear = 0.0;

    for (int a = 15; a <= 49; ++a) {
      popCurrent += c_dp.p_totpop(a, S_MALE);
      if (t > 1) {
        popLastYear += c_dp.p_totpop(a, S_MALE);
      }
    }

    real_type vmmcTarget = popCurrent * p_hv.rn_coverage(RN_MC15_49, t);

    int calcYearIdx = 1;
    real_type vmmcTraditional =
        c_dp.p_totpop(15, S_MALE) * p_hv.rn_coverage(RN_MC15_49, calcYearIdx);

    real_type vmmcLastYear = 0.0;
    if (t > 1) {
      real_type popExcludingUnder15 = popLastYear - c_dp.p_totpop(15, S_MALE);
      vmmcLastYear = popExcludingUnder15 * p_hv.rn_coverage(RN_MC15_49, t - 1);
    }

    real_type result = vmmcTarget - vmmcLastYear - vmmcTraditional;

    // Ensure we never return a negative number.
    result = std::max(result, 0.0);

    return result;
  }
};

}  // namespace internal
}  // namespace leapfrog
