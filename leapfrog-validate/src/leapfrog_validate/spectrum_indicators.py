"""Spectrum-comparison-track indicator registry: wires `diff_indicator` to Spectrum.

A second, parallel registry to `leapfrog_validate.indicators.INDICATORS` --
the same five blessed indicators (plus, since ticket 23, the tag-scoped
year-only `pmtct_need` and `cotrim_need`), same leapfrog-side extractors (imported, not
redefined: this module is read-only with respect to `indicators.py`, which
ticket 21 is working on in parallel on another branch), but each paired with
a Spectrum-side extractor (`leapfrog_validate.spectrum_extract`) and this
track's own, looser tolerance numbers (PRD: `rtol=1%` uniform, per-indicator
`atol`) instead of the regression track's tight ones.

`leapfrog_validate.diff.diff_indicator(arr_a, arr_b, name, tolerance, exclude)`
is array-in/array-out and doesn't care where `arr_b` came from -- so wiring
Spectrum in as "the comparison candidate" (this ticket's first checklist
item) needs no change to `diff.py` itself. `diff_spectrum_indicator` below
is the one new seam: given a PJNZ and a leapfrog `output.h5`, it extracts
both sides and calls `diff_indicator` with this track's `Tolerance`.

The PRD's illustrative registry sketch shows `spectrum: ...` as a field
alongside `leapfrog: ...` on each entry; `IndicatorSpec` in `indicators.py`
doesn't have that field (ticket 20 built it without one, since ticket 22/23
own adding it -- see that module's docstring), and editing it is out of
this ticket's scope (parallel-build collision avoidance, per the task
brief). Rather than extend that `TypedDict`, this module defines its own
`SpectrumIndicatorSpec` carrying both extractors plus this track's
tolerance/exclusions/scope -- reusing `Tolerance`/`Exclusion`/`Scope` (the
shared vocabulary, not the regression-track-specific dict) rather than
inventing parallel types for those.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import TypedDict

import numpy as np

from leapfrog_validate import indicators, spectrum_extract
from leapfrog_validate.diff import Tolerance, Verdict, diff_indicator
from leapfrog_validate.exclusions import Exclusion, exclusion_mask
from leapfrog_validate.indicators import ALL, Scope, tag

__all__ = [
    "ALL",
    "SPECTRUM_INDICATORS",
    "Scope",
    "SpectrumIndicatorSpec",
    "diff_spectrum_indicator",
]

# Spectrum-comparison track rtol is uniform across indicators (PRD's
# "Starting numbers" table) -- 1%, far looser than the regression track's
# 1e-6, since this compares two genuinely different implementations
# (leapfrog vs. Spectrum/AIM), not two builds of the same one.
SPECTRUM_RTOL = 0.01

# Per-indicator atol, explicit placeholders per the PRD (not yet
# empirically calibrated against the real private corpus -- "Empirically
# calibrated tolerance numbers" is listed in the PRD's own Out of Scope).
SPECTRUM_ATOL: dict[str, float] = {
    "total_population": 50.0,
    "hiv_population": 20.0,
    "treatment_population": 50.0,
    "aids_deaths_single_age": 1.0,
    "aids_deaths_on_treatment": 1.0,
    # Ticket 23's tag-scoped, year-only indicators: counts of mothers/children,
    # the same one-person scale as the deaths indicators. The `testthat`
    # comparisons they replace used absolute-only thresholds (1e-3 and 0.5)
    # sized for identical inputs; this track compares two different
    # implementations, so it uses its uniform rtol plus this small atol.
    "pmtct_need": 1.0,
    "cotrim_need": 1.0,
}


class SpectrumIndicatorSpec(TypedDict):
    """One blessed indicator's leapfrog + Spectrum extractors and this track's tolerance."""

    extract_leapfrog: Callable[[Path], np.ndarray]
    extract_spectrum: Callable[[Path], np.ndarray]
    tolerance: Tolerance
    exclusions: tuple[Exclusion, ...]
    scope: Scope


SPECTRUM_INDICATORS: dict[str, SpectrumIndicatorSpec] = {
    "total_population": {
        "extract_leapfrog": indicators.extract_total_population,
        "extract_spectrum": spectrum_extract.extract_spectrum_total_population,
        "tolerance": Tolerance(atol=SPECTRUM_ATOL["total_population"], rtol=SPECTRUM_RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "hiv_population": {
        "extract_leapfrog": indicators.extract_hiv_population,
        "extract_spectrum": spectrum_extract.extract_spectrum_hiv_population,
        "tolerance": Tolerance(atol=SPECTRUM_ATOL["hiv_population"], rtol=SPECTRUM_RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "treatment_population": {
        "extract_leapfrog": indicators.extract_treatment_population,
        "extract_spectrum": spectrum_extract.extract_spectrum_treatment_population,
        "tolerance": Tolerance(atol=SPECTRUM_ATOL["treatment_population"], rtol=SPECTRUM_RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "aids_deaths_single_age": {
        "extract_leapfrog": indicators.extract_aids_deaths_single_age,
        "extract_spectrum": spectrum_extract.extract_spectrum_aids_deaths_single_age,
        "tolerance": Tolerance(atol=SPECTRUM_ATOL["aids_deaths_single_age"], rtol=SPECTRUM_RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "aids_deaths_on_treatment": {
        "extract_leapfrog": indicators.extract_aids_deaths_on_treatment,
        "extract_spectrum": spectrum_extract.extract_spectrum_aids_deaths_on_treatment,
        "tolerance": Tolerance(atol=SPECTRUM_ATOL["aids_deaths_on_treatment"], rtol=SPECTRUM_RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    # Ticket 23: the two former hardcoded `test-child-model.R` comparisons, as
    # ordinary tag-scoped entries. Tags come from `classify.domain_tags`
    # (`has_pmtct`/`has_cotrim`: the PJNZ's PMTCT / cotrimoxazole input arrays
    # are not all zero). Year-only arrays: no sex or age axis.
    "pmtct_need": {
        "extract_leapfrog": indicators.extract_pmtct_need,
        "extract_spectrum": spectrum_extract.extract_spectrum_pmtct_need,
        "tolerance": Tolerance(atol=SPECTRUM_ATOL["pmtct_need"], rtol=SPECTRUM_RTOL),
        "exclusions": (),
        "scope": tag("has_pmtct"),
    },
    "cotrim_need": {
        "extract_leapfrog": indicators.extract_cotrim_need,
        "extract_spectrum": spectrum_extract.extract_spectrum_cotrim_need,
        "tolerance": Tolerance(atol=SPECTRUM_ATOL["cotrim_need"], rtol=SPECTRUM_RTOL),
        "exclusions": (),
        "scope": tag("has_cotrim"),
    },
}


def diff_spectrum_indicator(name: str, output_h5: Path, pjnz: Path, pjnz_label: str | None = None) -> Verdict:
    """Diff one indicator's leapfrog `output_h5` against `pjnz`'s own Spectrum modvars.

    `pjnz_label`, if given, is matched against the indicator's exclusion
    list (same convention as `cli.py`'s `_diff_one`) -- defaults to `None`
    (no exclusions applied), since no real `Exclusion` entry exists yet for
    this track (same "mechanism built, not yet populated" state ticket 20
    left the regression track's list in).
    """
    spec = SPECTRUM_INDICATORS[name]
    arr_leapfrog = spec["extract_leapfrog"](output_h5)
    arr_spectrum = spec["extract_spectrum"](pjnz)
    exclude = exclusion_mask(arr_leapfrog.shape, spec["exclusions"], pjnz_label) if pjnz_label is not None else None
    return diff_indicator(arr_leapfrog, arr_spectrum, name, spec["tolerance"], exclude=exclude)
