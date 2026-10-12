"""Indicator registry: leapfrog output.h5 variable(s) -> comparable array.

The five blessed indicators (per `.scratch/leapfrog-validation/PRD.md` and
ticket 02's `research/indicator-mapping` findings), each defined through one
common shape: a leapfrog extractor, the regression-track `atol`/`rtol` per
ticket 07's hybrid formula, an optional exclusion list (ticket 08's registry
pattern), and a PJNZ-scope selector (`Scope`/`ALL`/`tag(...)`, ticket 20).
Three indicators read a single output.h5 array directly; the other two
(`treatment_population`, `aids_deaths_on_treatment`) have no `p_`-level
array and are reconstructed by summing the adult (`h_`) and child
(`hc1_`/`hc2_`) arrays over CD4/duration and concatenating their age domains.

All extracted arrays share one (year, sex, age=81) shape, which
`leapfrog_validate.exclusions.exclusion_mask` relies on.

`Scope` restricts which PJNZ (by tag, as computed by `classify.classify`) an
indicator runs against -- the five blessed indicators default to `ALL` (every
PJNZ, unchanged behavior); a custom indicator can instead use `tag("some_tag")`
to run only where it's meaningful (ticket 20's acceptance case), e.g. the
`pmtct_need`/`cotrim_need` migration ticket 23 plans.
"""

from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import TypedDict

import h5py
import numpy as np

from leapfrog_validate.diff import Tolerance
from leapfrog_validate.exclusions import Exclusion

# Regression-track rtol is uniform across indicators (ticket 07's Answer).
RTOL = 1e-6

# h_/hc1_/hc2_ arrays are (year, sex, age, cd4, duration) -- summing the last
# two axes collapses CD4 stage and treatment-duration stage, leaving the same
# (year, sex, age) shape the four direct indicators already have.
_CD4_DURATION_AXES = (-2, -1)


def extract_total_population(output_h5: Path) -> np.ndarray:
    """Read the p_totpop array (year x sex x age) out of a raw output.h5."""
    with h5py.File(output_h5, "r") as f:
        return f["p_totpop"][()]


def extract_hiv_population(output_h5: Path) -> np.ndarray:
    """Read the p_hivpop array (year x sex x age) out of a raw output.h5.

    Covers adult and pediatric ages alike -- the child model updates
    `p_hivpop` directly at child single-year ages (ticket 02's Answer).
    """
    with h5py.File(output_h5, "r") as f:
        return f["p_hivpop"][()]


def extract_aids_deaths_single_age(output_h5: Path) -> np.ndarray:
    """Read the p_hiv_deaths array (year x sex x age) out of a raw output.h5.

    Already combines adult + child, on-ART + off-ART deaths (ticket 02's
    Answer) -- no reconstruction needed, unlike `aids_deaths_on_treatment`.
    """
    with h5py.File(output_h5, "r") as f:
        return f["p_hiv_deaths"][()]


def _extract_summed_by_age_domain(
    output_h5: Path,
    adult_var: str,
    child_0_4_var: str,
    child_5_14_var: str,
) -> np.ndarray:
    """Sum one adult (h_) and two child (hc1_/hc2_) arrays into one (year, sex, age=81) array.

    No `p_`-level array exists for these indicators, so the three age
    domains -- ages 0-4 (`hc1AG`=5), 5-14 (`hc2AG`=10), 15-80 (`hAG`=66) --
    are summed over CD4/duration and concatenated in increasing-age order to
    span the same 81 single-year ages as the direct indicators (ticket 02's
    `research/indicator-mapping` findings).
    """
    with h5py.File(output_h5, "r") as f:
        adult = f[adult_var][()]
        child_0_4 = f[child_0_4_var][()]
        child_5_14 = f[child_5_14_var][()]

    return np.concatenate(
        [
            child_0_4.sum(axis=_CD4_DURATION_AXES),
            child_5_14.sum(axis=_CD4_DURATION_AXES),
            adult.sum(axis=_CD4_DURATION_AXES),
        ],
        axis=2,
    )


def extract_treatment_population(output_h5: Path) -> np.ndarray:
    """Sum h_artpop + hc1_artpop + hc2_artpop across CD4/duration -> (year, sex, age=81)."""
    return _extract_summed_by_age_domain(output_h5, "h_artpop", "hc1_artpop", "hc2_artpop")


def extract_aids_deaths_on_treatment(output_h5: Path) -> np.ndarray:
    """Sum h_hiv_deaths_art + hc1_art_aids_deaths + hc2_art_aids_deaths -> (year, sex, age=81)."""
    return _extract_summed_by_age_domain(output_h5, "h_hiv_deaths_art", "hc1_art_aids_deaths", "hc2_art_aids_deaths")


@dataclass(frozen=True)
class Scope:
    """Which PJNZ an indicator applies to, by tag (ticket 20's PJNZ-scope selector).

    `tags=None` -- the `ALL` sentinel below -- means every PJNZ is in scope,
    unchanged behavior for the five blessed indicators. A concrete `Scope`
    (built via `tag(...)`) requires *every* listed tag to be a member of a
    PJNZ's tag set (from `classify.classify`) -- AND, not OR. That's the
    simplest rule that already covers ticket 23's planned `pmtct_need`
    (`tag("has_pmtct")`) / `cotrim_need` (`tag("has_cotrim")`) single-tag
    scopes, with no indicator today needing anything richer; widen this if a
    future indicator genuinely needs OR-of-tags.
    """

    tags: frozenset[str] | None = None

    def applies_to(self, pjnz_tags: frozenset[str]) -> bool:
        """Return whether a PJNZ carrying `pjnz_tags` is in scope for this indicator."""
        return self.tags is None or self.tags.issubset(pjnz_tags)


# The default scope for the five blessed indicators: every PJNZ, regardless
# of tags. Named to mirror the PRD's illustrative `scope: ALL` sketch.
ALL = Scope()


def tag(*names: str) -> Scope:
    """Build a `Scope` restricted to PJNZ carrying every one of `names`.

    Mirrors the PRD's illustrative `scope: tag("has_pmtct")` syntax.
    """
    if not names:
        msg = "tag() requires at least one tag name"
        raise ValueError(msg)
    return Scope(tags=frozenset(names))


class IndicatorSpec(TypedDict):
    """A leapfrog extractor plus the tolerance, exclusions, and PJNZ-scope it's judged against."""

    extract: Callable[[Path], np.ndarray]
    tolerance: Tolerance
    exclusions: tuple[Exclusion, ...]
    scope: Scope


INDICATORS: dict[str, IndicatorSpec] = {
    "total_population": {
        "extract": extract_total_population,
        "tolerance": Tolerance(atol=1e-3, rtol=RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "hiv_population": {
        "extract": extract_hiv_population,
        "tolerance": Tolerance(atol=1e-3, rtol=RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "treatment_population": {
        "extract": extract_treatment_population,
        "tolerance": Tolerance(atol=1e-3, rtol=RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "aids_deaths_single_age": {
        "extract": extract_aids_deaths_single_age,
        "tolerance": Tolerance(atol=1e-4, rtol=RTOL),
        "exclusions": (),
        "scope": ALL,
    },
    "aids_deaths_on_treatment": {
        "extract": extract_aids_deaths_on_treatment,
        "tolerance": Tolerance(atol=1e-4, rtol=RTOL),
        "exclusions": (),
        "scope": ALL,
    },
}
