"""Reviewable, per-PJNZ carve-outs for a known, explained tolerance failure.

Each entry is scoped to one PJNZ and requires both a reason and a link to the
underlying explanation, so the list can't quietly become a dumping ground for
unexplained failures (per ticket 07's Answer and
`.scratch/leapfrog-validation/issues/17-full-indicator-registry-tolerance-rollup.md`).
An entry carves out a (year, sex, age) region of one indicator's array --
mirroring the `Age < 4 & Year < 2030`-style filters `leapfrogr`'s own tests
already use for known-divergent regions -- rather than loosening tolerance
globally.
"""

from collections.abc import Sequence
from dataclasses import dataclass

import numpy as np

# An inclusive (min, max) index bound -- unlike Python's own half-open range().
InclusiveBounds = tuple[int, int]


@dataclass(frozen=True)
class Exclusion:
    """A carved-out (year, sex, age) region of one indicator, for one PJNZ.

    `year`/`sex`/`age` are inclusive (min, max) index bounds; `None` means
    "every index on that axis". Omitting all three excludes the indicator's
    entire array for `pjnz`.
    """

    pjnz: str
    reason: str
    link: str
    year: InclusiveBounds | None = None
    sex: InclusiveBounds | None = None
    age: InclusiveBounds | None = None

    def __post_init__(self) -> None:
        """Reject an entry missing a reason or a link."""
        if not self.reason.strip():
            msg = "Exclusion requires a non-empty reason"
            raise ValueError(msg)
        if not self.link.strip():
            msg = "Exclusion requires a non-empty link"
            raise ValueError(msg)


def _axis_mask(size: int, bounds: InclusiveBounds | None) -> np.ndarray:
    if bounds is None:
        return np.ones(size, dtype=bool)
    lo, hi = bounds
    idx = np.arange(size)
    return (idx >= lo) & (idx <= hi)


def exclusion_mask(shape: tuple[int, ...], exclusions: Sequence[Exclusion], pjnz: str) -> np.ndarray:
    """Boolean mask, True where a cell is excluded for `pjnz`.

    `shape` is either `(year, sex, age)` or, for a year-only indicator such
    as `pmtct_need`/`cotrim_need`, just `(year,)`. A year-only indicator has
    no sex or age axis, so an entry carrying a `sex` or `age` bound is
    rejected for it rather than silently ignored.
    """
    if len(shape) == 1:
        return _year_only_exclusion_mask(shape[0], exclusions, pjnz)
    n_year, n_sex, n_age = shape
    mask = np.zeros(shape, dtype=bool)
    for excl in exclusions:
        if excl.pjnz != pjnz:
            continue
        region = (
            _axis_mask(n_year, excl.year)[:, None, None]
            & _axis_mask(n_sex, excl.sex)[None, :, None]
            & _axis_mask(n_age, excl.age)[None, None, :]
        )
        mask |= region
    return mask


def _year_only_exclusion_mask(n_year: int, exclusions: Sequence[Exclusion], pjnz: str) -> np.ndarray:
    mask = np.zeros(n_year, dtype=bool)
    for excl in exclusions:
        if excl.pjnz != pjnz:
            continue
        for axis, bounds in (("sex", excl.sex), ("age", excl.age)):
            if bounds is not None:
                msg = f"Exclusion for '{pjnz}' sets a {axis} bound, but this indicator has no {axis} axis"
                raise ValueError(msg)
        mask |= _axis_mask(n_year, excl.year)
    return mask
