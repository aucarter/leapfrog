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
| 1 | ART targets within the year | interpolated as for a mid-year projection whatever the projection period | by projection period | to do |
| 2 | ART initiation by CD4 stage | one step per cell: target x eligible x (w mortality share + (1 - w) eligibility share), capped at the eligible population | `epp_age_model` allocation | to do |
| 3 | Order within a time step | untreated update, then ART deaths and duration, then dropout from the updated ART stock, before initiation | leapfrog's order | to do |
| 4 | HIV+ migrants | an input by year, sex and age group replaces net migration x HIV+ population in years it is non-zero | net migration rate x HIV+ population | to do |
| 5 | Diagnosis | diagnosed stock by CD4, age group and sex, with diagnosis rates as an input | none | to do |

Inputs, not engine (set by the caller): ART dropout is the annual proportion the DLL applies; dropouts recover one CD4
stage (`art_dropout_recover_cd4 = TRUE`, as eppasm).

## Evidence

- **Starting point.** leapfrog with `epp_age_model = 1`, `csavr_dll_model = 0` and the same incidence, IRRs, entrants
  and `art_dropout_recover_cd4` equals eppasm's per-step direct incidence on PAN 2026 (family 1): PLHIV within
  1.1e-5, AIDS deaths within 8e-5, 1970-2020.
- **Row 0.** Against eppasm's annual direct incidence with the DLL's previous-year weights (PAN 2026, family 1): new
  infections within 1e-9, PLHIV within 5e-5, AIDS deaths within 1.7e-4, 1971-2025.
- **Off is unchanged.** `csavr_dll_model = 0` on this build reproduces the starting build's PAN comparison to every
  printed digit.
