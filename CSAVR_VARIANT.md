# `csavr_dll_model`: reproducing Spectrum's CSAVR engine

`csavr_dll_model` is one integer parameter in `HaConfig` (R name `csavr_dll_model`; `process_pjnz()` sets 0). With 1,
leapfrog behaves as the projection inside Spectrum's CSAVR engine (`eppcsavri686.dll`, Spectrum 6.44), which
is eppasm's age model plus a few behaviours of its own. It is meant to be used with `epp_age_model = 1`,
direct incidence input (`incidence_model_choice` direct) and the coarse static-entrant variant
(`HivCoarseAgeStratificationEntrants`).

- **0** (default): unchanged.
- **1**: the CSAVR engine's projection.

Branch `feature/csavr-variant` on aucarter/leapfrog, from `feature/epp-age-model` (`d70b7bef`).

## What the switch changes

Each row is one behaviour of the DLL, read from its decompile and confirmed against a patched eppasm
(leapfrogfit `scripts/csavr/patch_eppasm_dll.py`, `docs/CSAVR_REPLICATION.md`).

| # | Area | `csavr_dll_model = 1` (CSAVR DLL) | `0` | Status |
|---|---|---|---|---|
| 0 | New infections | added once per year, after the time steps (eppasm `EPP_DIRECTINCID_ANN`); age distribution weights last year's HIV-negative population at each single age, rate clamped to [0, 1], capped at this year's HIV-negative population | spread over the time steps, weighted by the current population | done |
| 1 | ART targets within the year | interpolated as for a mid-year projection whatever the projection period | by projection period | done |
| 2 | ART initiation by CD4 stage | one step per age x CD4 cell, each cell weighted by its own age-specific mortality: target x eligible x (w mort / expected mortality + (1 - w) / eligible), capped at the eligible and available population | stage shares, then split across ages by eligibility (stage-average mortality) | done |
| 3 | Order within a time step | untreated update, then ART deaths and duration, then dropout from the updated ART stock, before initiation | leapfrog's order | done |
| 4 | HIV+ migrants | in a year where the input (`hivmigr`, [8 groups 15-19 ... 45-49, 50+, sex, year]) is non-zero anywhere, it replaces net migration x HIV+ population at ages 15+, spread evenly over each group's single ages (15-19 for the 15-16 and 17-19 groups); HIV-negative people still migrate at the net rate | net migration rate x HIV+ population | done |
| 5 | Diagnosis | diagnosed stock `h_diagpop` [CD4, age group, sex], from year index `diag_start`, rates `diag_rate` [CD4, age group, sex, year] as input: ages with the untreated stock (nobody enters at 15); background deaths and migration at the group's rates, migration applied twice after the start year (with the HIV+ migrant input it gains untreated x rate first); each time step before ART, diagnosis of the undiagnosed and the untreated CD4 progression and unscaled mortality; dropouts return to it; initiations come from it, with any excess taken from the age group's diagnosed pro rata; held within [0, untreated + on ART] at year end | none | done |

Inputs, not engine (set by the caller):
- ART dropout is the annual proportion the DLL applies.
- Dropouts recover one CD4 stage (`art_dropout_recover_cd4 = TRUE`, as eppasm).
- **Open age group survival.** The DLL ages ages 15-79 up one year and adds last year's 80+ into the open
  group, then applies one survival to all of 80+ (decompile `at_63adae80.c` ~L856-878: 65 copies, then
  `+=` at offset 0x208 = age 80). Its survival array has 66 entries (15-80), so there is no 79-to-80 survival.
  In leapfrog this is `Sx[81, , ] <- Sx[82, , ]`, as leapfrogfit's `epp_open_age_survival` sets for EPP.
- **Untreated mortality scaling on.** The DLL scales untreated CD4 mortality by the untreated share when a flag
  it receives from Spectrum is set (`initialize_from_c`: `S_FP_C + 0x1e0` -> `this + 0x1be8`, read at
  `at_63adae80.c` ~L1558, gating the scale at ~L1619). URY's DP holds `scale_cd4_mort = 0`, yet Spectrum's
  stored CSAVR outputs need the scaling on (PLHIV 0.2% vs 5% off): Spectrum passes it on whatever the DP says
  (inferred; Spectrum.exe is not decompiled). The caller sets `scale_cd4_mort = 1`.

New required parameters: `csavr_dll_model` (int), `hivmigr` ([8, 2, years]), `diag_start` (int, 0-based year index)
and `diag_rate` ([hDS, hAG, 2, years]); `process_pjnz()` sets 0, zeros, the projection length (never) and zeros.
Callers that build parameters without it must add both.

## Evidence

- **Starting point.** leapfrog with `epp_age_model = 1`, `csavr_dll_model = 0` and the same incidence, IRRs, entrants
  and `art_dropout_recover_cd4` equals eppasm's per-step direct incidence on PAN 2026 (family 1): PLHIV within
  1.1e-5, AIDS deaths within 8e-5, 1970-2020.
- **Row 0.** Against eppasm's annual direct incidence with the DLL's previous-year weights (PAN 2026, family 1),
  with the open-age survival above: PLHIV, new infections, AIDS deaths, on-ART and total population within 9e-16
  every year 1971-2030; worst single age x sex x year PLHIV cell 4.7e-15. Without the open-age survival, deaths
  drift to 1.7e-4 by 2025 (80+ population up to 9.7% off).
- **Rows 0-1.** Against the oracle with `dll_art_midyear` added (PAN 2026, family 1): PLHIV and AIDS deaths within
  9e-16 every year 1971-2030; worst single cell 5.1e-15.
- **Rows 0-2.** With `dll_art_cellalloc` added: PLHIV and deaths within 9e-16; worst single cell 4.2e-15.
- **Rows 0-3.** With `dll_steporder` added: PLHIV and deaths within 1.2e-15; worst single cell 4e-15.
- **Rows 0-3, all files.** 35 CSAVR-fit 2026 files x families 1-3, 5, 6 (Slurm array 409407): 170 of 175 within
  1.5e-14 in PLHIV, deaths, infections, on-ART, total population and every age x sex x year PLHIV cell. The other
  five were URY, whose DP turns the untreated mortality scaling off (above); with it on, URY family 1 is within 7e-16.
- **Rows 0-4, all files.** 35 files x families 1-3, 5, 6 (Slurm array 436773): totals (PLHIV, deaths, infections,
  on-ART, population) within 4.2e-15; every age x sex x year PLHIV cell within 1.9e-13 (largest in ALB, 2-46
  infections a year).
- **Row 5.** PAN, families 1-3, 5, 6, with the oracle's diagnosis rates (`l2_diag_rates` from the base run's ART
  coverage and Spectrum's stored diagnosis parameters): diagnosed total within 1.6e-14, reported new diagnoses within
  6.7e-16, largest absolute difference in any diagnosed cell 5.9e-13 persons.
- **Off is unchanged.** `csavr_dll_model = 0` on this build reproduces the starting build's PAN comparison to every
  printed digit.
