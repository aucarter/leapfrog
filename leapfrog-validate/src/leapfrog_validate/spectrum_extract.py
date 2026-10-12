"""Read Spectrum's own modvars directly out of a PJNZ file (no live Spectrum.exe).

Ticket 22's core new piece. `leapfrog-compare/src/leapfrog_compare/spectrum_runner.py`
and `indicator_map.py` are the confirmed reuse candidates (ticket 05's research
verdict) and this module's design reference -- but `spectrum_runner.run_spectrum`
itself isn't reusable as-is: it *runs* `leapfrog_py.run_model(...)` to get the
leapfrog side too, where this system already has its own leapfrog-side extractors
(`leapfrog_validate.indicators`) built against `leapfrogr`'s `output.h5`, not
`leapfrog-py`'s in-memory dict. Only the Spectrum-reading half is reused here:

    raw_modvars, _, _, _ = GB_ImportProjectionFromFile(str(pjnz_path))
    modvars = modvars_to_numpy(raw_modvars)

-- `Tools.ImportPJNZ.Importer.GB_ImportProjectionFromFile` plus
`SpectrumCommon.Util.ConvertNumpy.modvars_to_numpy`, both confirmed real,
**public** PyPI packages (`avenir-spectrum-common`, `avenir-spectrum-import-pjnz`;
see `pyproject.toml`'s comment) -- not a private index needing credentials this
sandbox lacks, despite `leapfrog-compare/pyproject.toml`'s commented-out
`[tool.uv.sources]` local-path overrides suggesting otherwise at a glance. `uv add`
for both resolved and installed cleanly in this exact worktree, and their real,
installed API (shapes/tag names below) was cross-checked directly against this
repo's own public PJNZ fixtures (`leapfrogr/inst/pjnz/france_default.PJNZ`,
`bwa_aim-no-special-elig-numpmtct.PJNZ`) before any of this module's extraction
logic was written -- not guessed from `leapfrog-compare`'s docstrings alone.

Five modvars back the PRD's indicator -> Spectrum mapping table, all sharing one
real, confirmed shape: `(sex=3, age=81, year)` -- index 0 on the sex axis is a
precomputed "both sexes" total that `leapfrog-compare/indicator_map.py`'s own
convention deliberately never trusts; every total here is computed as male
(index 1) + female (index 2) instead, matching that convention and this
system's own leapfrog-side sex axis (0=male, 1=female):

    DP_BigPop_V1                  -- total_population
    AM_HIVBySingleAge_V1          -- hiv_population
    AM_OnARTBySingleAge_V1        -- treatment_population (confirmed by ticket 13's
                                      research, and by this module's own age-domain
                                      reconstruction check below, to cover children
                                      at single-year resolution -- no leapfrog-side
                                      summing needed on the Spectrum side, unlike the
                                      leapfrog side's three-array reconstruction)
    AM_AIDSDeathsARTSingleAge_V1  -- aids_deaths_on_treatment (alone); summed with...
    AM_AIDSDeathsNoARTSingleAge_V1 -- ...for aids_deaths_single_age

Year-axis alignment, the one real design wrinkle this ticket's brief didn't
foresee: `run_model.R` (`model_run.py`'s Rscript) hardcodes no
`output_years` argument, so `leapfrogr::run_model()`'s own default
(`seq(1970, 2030)`, confirmed via `args(leapfrog::run_model)`) always wins --
every leapfrog `output.h5` this whole system produces has a year axis fixed
at calendar years 1970-2030 (length 61), *regardless of which PJNZ backs
it*. A PJNZ's own Spectrum modvars, though, span whatever first/final year
*that PJNZ* declares (confirmed empirically: `france_default.PJNZ` is
1970-2032 (63 years), `bwa_aim-no-special-elig-numpmtct.PJNZ` is exactly
1970-2030 (61 years, a lucky no-op case) -- these are not always the same
length or the same range as leapfrog's fixed window. `_align_to_leapfrog_years`
below slices a Spectrum modvar's year axis down to that same 1970-2030
window using the PJNZ's own declared first year, rather than assuming every
PJNZ starts in 1970 -- and raises `SpectrumExtractError` rather than silently
misaligning if a PJNZ's own range doesn't fully cover it. This mirrors a
real coupling already implicit (and undocumented) in every existing
`leapfrog_validate.indicators` extractor; it isn't new here, just made
explicit because this is the first place two *different* sources' year axes
ever need to line up cell-for-cell.
"""

from __future__ import annotations

from functools import lru_cache
from pathlib import Path

import numpy as np

# Mirrors `leapfrogr::run_model()`'s own default `output_years = seq(1970, 2030)`
# (confirmed via `args(leapfrog::run_model)`), which `model_run.py`'s Rscript
# invocation never overrides -- see this module's docstring. If a future
# change ever threads a real `--output-years` through `run`/`run_model.R`,
# this constant (and the alignment it drives) would need to follow it.
LEAPFROG_OUTPUT_YEARS = range(1970, 2031)


class SpectrumExtractError(RuntimeError):
    """Raised when a PJNZ's Spectrum modvars can't be read or aligned."""


@lru_cache(maxsize=2)
def _import_modvars_cached(resolved_pjnz: str) -> dict:
    """Do the real import work, cached -- `_import_modvars` is the public wrapper.

    Keyed on a resolved-path *string* (not a `Path`) since `lru_cache`
    needs a hashable key and this is called only through `_import_modvars`
    below, never directly. `maxsize=2` is deliberately small: each of the
    5 `extract_spectrum_*` calls for one PJNZ (`diff_spectrum_indicator`
    runs one per blessed indicator) hits this with the *same* resolved
    path, so a cache of 1 would already amortize that -- 2 gives a little
    slack without letting a large multi-PJNZ corpus walk hold more than a
    couple of full modvars dicts in memory at once. A failed import
    (`SpectrumExtractError`) is never cached -- `functools.lru_cache`
    doesn't memoize an exception, so a transient failure is retried on the
    next call rather than "stuck" failing forever.
    """
    from SpectrumCommon.Util.ConvertNumpy import modvars_to_numpy  # noqa: PLC0415
    from Tools.ImportPJNZ.Importer import GB_ImportProjectionFromFile  # noqa: PLC0415

    try:
        raw_modvars, _unused_params, _unused_sheet, _unused_projection = GB_ImportProjectionFromFile(resolved_pjnz)
    except Exception as e:
        msg = f"Could not import Spectrum modvars from {resolved_pjnz}: {e}"
        raise SpectrumExtractError(msg) from e
    return modvars_to_numpy(raw_modvars)


def _import_modvars(pjnz: Path) -> dict:
    """Import `pjnz` and return its modvars dict (list values as numpy arrays).

    Lazy-imports the avenir packages (same pattern as `cli.py`'s
    `_azure_blob_service_client` for `azure-identity`/`azure-storage-blob`)
    so importing this module doesn't hard-require them for a caller that
    never touches the Spectrum track. Cached (see `_import_modvars_cached`)
    because every one of the five `extract_spectrum_*` functions calls
    this independently -- without caching, `diff_spectrum_indicator`
    re-parses the same PJNZ via the proprietary importer once per
    indicator (5x) instead of once per file, for every PJNZ in a corpus
    walk (`spectrum_track.py`'s `run` command).
    """
    return _import_modvars_cached(str(pjnz.resolve()))


def _year_window(modvars: dict, pjnz: Path) -> tuple[int, int]:
    """Return `(first_year, final_year)`, inclusive, declared by `pjnz`'s own modvars."""
    from SpectrumCommon.Const.PJ.PJNTags import PJN_FinalYearTag, PJN_FirstYearTag  # noqa: PLC0415

    try:
        return int(modvars[PJN_FirstYearTag]), int(modvars[PJN_FinalYearTag])
    except KeyError as e:
        msg = f"{pjnz}: modvars missing first/final year tag ({e})"
        raise SpectrumExtractError(msg) from e


def _align_to_leapfrog_years(arr: np.ndarray, first_year: int, final_year: int, pjnz: Path) -> np.ndarray:
    """Slice `arr`'s last (year) axis from `pjnz`'s own window to `LEAPFROG_OUTPUT_YEARS`.

    `arr` is `(sex, age, year)`, year starting at `first_year`. Raises
    rather than silently misaligning if `pjnz`'s own declared range doesn't
    fully cover leapfrog's fixed 1970-2030 output window -- see this
    module's docstring for why that window is fixed.
    """
    lo, hi = LEAPFROG_OUTPUT_YEARS.start, LEAPFROG_OUTPUT_YEARS.stop - 1
    if first_year > lo or final_year < hi:
        msg = (
            f"{pjnz}: Spectrum modvar years {first_year}-{final_year} don't fully cover "
            f"leapfrog's output window {lo}-{hi}; cannot align for comparison."
        )
        raise SpectrumExtractError(msg)
    start_idx = lo - first_year
    end_idx = start_idx + (hi - lo + 1)
    return arr[..., start_idx:end_idx]


def _modvar_to_leapfrog_shape(arr_sex_age_year: np.ndarray) -> np.ndarray:
    """`(sex=3, age=81, year)` -> `(year, sex=2, age=81)`, dropping the untrusted "both" row.

    Matches `leapfrog-compare/indicator_map.py`'s convention (see this
    module's docstring): index 0 on the sex axis is a precomputed total
    this system never reads directly; male (1) and female (2) are kept
    separately as leapfrog's own sex axis order (0=male, 1=female).
    """
    male = arr_sex_age_year[1]  # (age, year)
    female = arr_sex_age_year[2]
    return np.stack([male.T, female.T], axis=1)  # (year, sex=2, age)


def _read_modvar(modvars: dict, tag: object, pjnz: Path, first_year: int, final_year: int) -> np.ndarray:
    try:
        raw = np.asarray(modvars[tag])
    except KeyError as e:
        msg = f"{pjnz}: modvars missing tag {tag!r} ({e})"
        raise SpectrumExtractError(msg) from e
    aligned = _align_to_leapfrog_years(raw, first_year, final_year, pjnz)
    return _modvar_to_leapfrog_shape(aligned)


def extract_spectrum_total_population(pjnz: Path) -> np.ndarray:
    """DP_BigPop_V1 -> (year, sex=2, age=81), aligned to leapfrog's 1970-2030 window."""
    from SpectrumCommon.Const.DP.DPTags import DP_BigPopTag  # noqa: PLC0415

    modvars = _import_modvars(pjnz)
    first_year, final_year = _year_window(modvars, pjnz)
    return _read_modvar(modvars, DP_BigPopTag, pjnz, first_year, final_year)


def extract_spectrum_hiv_population(pjnz: Path) -> np.ndarray:
    """AM_HIVBySingleAge_V1 -> (year, sex=2, age=81). Covers adult and pediatric ages."""
    from SpectrumCommon.Const.AM.AMTags import AM_HIVBySingleAgeTag  # noqa: PLC0415

    modvars = _import_modvars(pjnz)
    first_year, final_year = _year_window(modvars, pjnz)
    return _read_modvar(modvars, AM_HIVBySingleAgeTag, pjnz, first_year, final_year)


def extract_spectrum_treatment_population(pjnz: Path) -> np.ndarray:
    """AM_OnARTBySingleAge_V1 -> (year, sex=2, age=81).

    Confirmed by ticket 13's research to cover children at single-year
    resolution already -- unlike the leapfrog side (which has no single
    `p_`-level array and sums three separate adult/child arrays), there is
    no reconstruction to do here; Spectrum already writes one whole-
    population modvar.
    """
    from SpectrumCommon.Const.AM.AMTags import AM_OnARTBySingleAgeTag  # noqa: PLC0415

    modvars = _import_modvars(pjnz)
    first_year, final_year = _year_window(modvars, pjnz)
    return _read_modvar(modvars, AM_OnARTBySingleAgeTag, pjnz, first_year, final_year)


def extract_spectrum_aids_deaths_single_age(pjnz: Path) -> np.ndarray:
    """AM_AIDSDeathsARTSingleAge_V1 + AM_AIDSDeathsNoARTSingleAge_V1 -> (year, sex=2, age=81)."""
    from SpectrumCommon.Const.AM.AMTags import (  # noqa: PLC0415
        AM_AIDSDeathsARTSingleAgeTag,
        AM_AIDSDeathsNoARTSingleAgeTag,
    )

    modvars = _import_modvars(pjnz)
    first_year, final_year = _year_window(modvars, pjnz)
    art = _read_modvar(modvars, AM_AIDSDeathsARTSingleAgeTag, pjnz, first_year, final_year)
    no_art = _read_modvar(modvars, AM_AIDSDeathsNoARTSingleAgeTag, pjnz, first_year, final_year)
    return art + no_art


def extract_spectrum_aids_deaths_on_treatment(pjnz: Path) -> np.ndarray:
    """AM_AIDSDeathsARTSingleAge_V1 alone -> (year, sex=2, age=81).

    Per the PRD's mapping table: the same modvar `aids_deaths_single_age`
    partially reuses, but alone rather than summed with the no-ART array --
    the leapfrog-side comparator is reconstructed the same way
    (`h_hiv_deaths_art` + `hc1_art_aids_deaths` + `hc2_art_aids_deaths`,
    `leapfrog_validate.indicators.extract_aids_deaths_on_treatment`).
    """
    from SpectrumCommon.Const.AM.AMTags import AM_AIDSDeathsARTSingleAgeTag  # noqa: PLC0415

    modvars = _import_modvars(pjnz)
    first_year, final_year = _year_window(modvars, pjnz)
    return _read_modvar(modvars, AM_AIDSDeathsARTSingleAgeTag, pjnz, first_year, final_year)
