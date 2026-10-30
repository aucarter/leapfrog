# `epp_age_model`: reproducing EPP's age model

`epp_age_model` is one integer parameter in `HaConfig` (R name `epp_age_model`; `process_pjnz()` sets 0). It controls
whether leapfrog behaves as the age model inside EPP 2025_R1 (`AgeModel.run_hiv_model`, which eppasm also implements).

- **0** (default): unchanged. Bit-identical to the starting build, `11edfa44` (see "Evidence").
- **1**: EPP's age model. With EPP's inputs (see "Inputs"), leapfrog reproduces EPP's own projection at EPP's own
  posterior draws to floating-point precision.

Branch `feature/epp-age-model` on aucarter/leapfrog. It sits on `fix/epp-entrant-migrants` (11 commits), which is on
`11edfa44`, 29 commits on top of hivtools `main` `e42e108c`.

## Correcting EPP's bugs: `epp_fix_bugs`

A second parameter, `epp_fix_bugs` (default 0), keeps `epp_age_model = 1` but corrects EPP's one engine-side bug:
entrants on paediatric ART before adult ART starts (row 6 below) join the untreated pool instead of leaving the
CD4-stratified population. EPP's other bugs are in its inputs and its sampler; leapfrogfit corrects them
(`epp_bug_fixes`, `docs/EPP_BUGS.md`), so EPP, EPP with its bugs fixed, and leapfrog's own model can be compared.

## What the switch changes

Each row is one behaviour: what EPP does, where in EPP's code, and what leapfrog does with 0.

| # | Area | `epp_age_model = 1` (EPP, eppasm) | `0` (leapfrog) | EPP source |
|---|---|---|---|---|
| 1 | Static entrants: paediatric survival | `paedsurv_lag` read at the previous year | current year | `run_hiv_model`, entrants block |
| 2 | Static entrants: net migrants | HIV-negative entrants `birthslag*cumsurv*(1 - prev/paedsurv) + cumnetmigr*(1 - pregprevlag*netmig_hivprob)`; migrants kept apart from children who died of HIV; `pregprev` computed in-model and lagged 15 years | migrants folded into the cohort and split by `prev/paedsurv` | same |
| 3 | Static entrants: survival at 15 | the cohort, net migrants included, survives the year at 15 after the entrants are built | migrants not survived | entrants, then natural deaths |
| 4 | Static entrants: births | births taken from the age-15 women *after* they are replaced | before (so they include children who died of HIV) | `births_by_ha` after the entrants |
| 5 | Static entrants: HIV+ entrants' deaths | HIV+ entrants enter before the year's natural deaths; one rate for the age group | entered already survived; the group's rate thinned them twice | `deathmigrate_ha` |
| 6 | Static entrants on ART before adult ART starts | the on-ART share joins no CD4-stratified pool (single-age HIV+ only) | joins the untreated pool | entrants block, `t <= t_ART_start` |
| 7 | Pregnant women's ART eligibility: births | the year's births, before its HIV deaths | births recomputed every time step from `p_totpop`, which loses HIV deaths | `births_by_ha` |
| 8 | Pregnant women's ART eligibility: HIV-negative women | after the step's infections | before | `pop[t][0]` at the ART step |
| 9 | ART initiation by CD4 stage | eligibility and expected-mortality shares mixed, allocated once (`art_alloc_method` 3) | Spectrum 6.43: each scheme allocates separately, then averaged | ART block |

Rows 1-6 apply only to the `*Entrants` variants (`run_static_entrants`). Rows 7-9 apply to every variant.

## Inputs EPP uses that are not the DP's

Exact reproduction also needs EPP's inputs. leapfrogfit builds them (`unaids_epp_asm` profile); they are not in
leapfrog. EPP reads Spectrum from the copy stored with its workset, `<project>_ep5.bak`, not the DP:

- **Survival:** `<SurvRate MV2>`, stored to 5 decimals (the DP has 7-8), with the open group's survival for everyone
  80+ (EPP has no 79-to-80 survival).
- **Entrants:** entrant prevalence over the snapshot's age-14 `<BigPop MV>`; paediatric survival from its
  `<AidsDeathsByAge MV2>` / `<HIVBySingleAge MV2>`; `cumnetmigr` from EPP's under-5 migration, which its parser
  Beers-interpolates from 1975.
- **Subpopulations:** regional structure raked to the snapshot's `<BigPop MV>`.
- **Census FRR adjustment:** applied inside the model's fertility ratios.

Details and measured effects are in leapfrogfit `docs/EPP_BUGS.md`.

## Evidence

- **Off equals the starting build.** The same inputs go through the `11edfa44` build and this build with
  `epp_age_model = 0`. That covers leapfrog's bundled BWA PJNZ under the full- and coarse-age adult variants and both
  `*Entrants` variants, plus MWI 2026's fit parameters (pregnant women eligible). Largest difference in every output:
  exactly 0. The only addition is the `pregprev_lag` state.
- **On equals EPP.** EPP 2025_R1 was run headless on the UNAIDS 2026 worksets with an instrumented `AgeModel` that
  reproduces EPP's stored prevalence exactly. Our state was then compared year by year at EPP's own posterior draws
  (15-49 prevalence, populations, CD4 and ART pools, infections, deaths):
  - MWI national, two distant draws: within 1.5e-10 percentage points every year 1980-2020.
  - ZWE Mashonaland East: within 4e-14.
- **Test suites.** No failures in the leapfrog suite; the remaining errors are the 26 pre-existing fixture errors
  (`specpop_percelig` missing from test fixtures). `test-hiv-model.R` "static age-15 entrants" covers rows 2, 3 and 6
  under both settings.
