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
| 4 | HIV+ migrants | an input by year, sex and age group replaces net migration x HIV+ population in years it is non-zero | net migration rate x HIV+ population | to do |
| 5 | Diagnosis | diagnosed stock by CD4, age group and sex, with diagnosis rates as an input | none | to do |

Inputs, not engine (set by the caller):
- ART dropout is the annual proportion the DLL applies.
- Dropouts recover one CD4 stage (`art_dropout_recover_cd4 = TRUE`, as eppasm).
- **Open age group survival.** The DLL ages ages 15-79 up one year and adds last year's 80+ into the open
  group, then applies one survival to all of 80+ (decompile `at_63adae80.c` ~L856-878: 65 copies, then
  `+=` at offset 0x208 = age 80). Its survival array has 66 entries (15-80), so there is no 79-to-80 survival.
  In leapfrog this is `Sx[81, , ] <- Sx[82, , ]`, as leapfrogfit's `epp_open_age_survival` sets for EPP.

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
- **Off is unchanged.** `csavr_dll_model = 0` on this build reproduces the starting build's PAN comparison to every
  printed digit.
