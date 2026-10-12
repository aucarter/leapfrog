"""Pure-Python tests for the indicator registry's extractors.

Uses small synthetic output.h5 fixtures -- no R build needed, mirroring
test_diff.py's approach for the tolerance/rollup math.
"""

from pathlib import Path

import h5py
import numpy as np
import pytest

from leapfrog_validate.diff import Tolerance
from leapfrog_validate.indicators import (
    ALL,
    INDICATORS,
    Scope,
    extract_aids_deaths_on_treatment,
    extract_aids_deaths_single_age,
    extract_hiv_population,
    extract_total_population,
    extract_treatment_population,
    tag,
)


def _write_h5(path: Path, **datasets: np.ndarray) -> Path:
    with h5py.File(path, "w") as f:
        for name, array in datasets.items():
            f.create_dataset(name, data=array)
    return path


def _art_like(shape: tuple[int, int, int, int, int], fill: float) -> np.ndarray:
    return np.full(shape, fill, dtype=float)


def test_extract_total_population_reads_p_totpop(tmp_path):
    arr = np.arange(3 * 2 * 4, dtype=float).reshape(3, 2, 4)
    output = _write_h5(tmp_path / "output.h5", p_totpop=arr)
    np.testing.assert_array_equal(extract_total_population(output), arr)


def test_extract_hiv_population_reads_p_hivpop(tmp_path):
    arr = np.arange(3 * 2 * 4, dtype=float).reshape(3, 2, 4)
    output = _write_h5(tmp_path / "output.h5", p_hivpop=arr)
    np.testing.assert_array_equal(extract_hiv_population(output), arr)


def test_extract_aids_deaths_single_age_reads_p_hiv_deaths(tmp_path):
    arr = np.arange(3 * 2 * 4, dtype=float).reshape(3, 2, 4)
    output = _write_h5(tmp_path / "output.h5", p_hiv_deaths=arr)
    np.testing.assert_array_equal(extract_aids_deaths_single_age(output), arr)


def test_extract_treatment_population_sums_cd4_duration_and_concatenates_age_domains(tmp_path):
    n_year, n_sex = 2, 2
    # hc1AG=2 (ages 0-4 domain), hc2AG=3 (ages 5-14 domain), hAG=4 (adult domain)
    h_artpop = _art_like((n_year, n_sex, 4, 7, 3), fill=1.0)
    hc1_artpop = _art_like((n_year, n_sex, 2, 7, 3), fill=2.0)
    hc2_artpop = _art_like((n_year, n_sex, 3, 6, 3), fill=3.0)
    output = _write_h5(tmp_path / "output.h5", h_artpop=h_artpop, hc1_artpop=hc1_artpop, hc2_artpop=hc2_artpop)

    result = extract_treatment_population(output)

    assert result.shape == (n_year, n_sex, 2 + 3 + 4)
    # each cell is fill_value * n_cd4 * n_dur, summed over those two axes;
    # age-domain order must be hc1 (0-4), hc2 (5-14), h (15-80)
    np.testing.assert_allclose(result[..., :2], 2.0 * 7 * 3)
    np.testing.assert_allclose(result[..., 2:5], 3.0 * 6 * 3)
    np.testing.assert_allclose(result[..., 5:], 1.0 * 7 * 3)


def test_extract_aids_deaths_on_treatment_sums_cd4_duration_and_concatenates_age_domains(tmp_path):
    n_year, n_sex = 2, 2
    h = _art_like((n_year, n_sex, 4, 7, 3), fill=0.5)
    hc1 = _art_like((n_year, n_sex, 2, 7, 3), fill=0.25)
    hc2 = _art_like((n_year, n_sex, 3, 6, 3), fill=0.1)
    output = _write_h5(
        tmp_path / "output.h5",
        h_hiv_deaths_art=h,
        hc1_art_aids_deaths=hc1,
        hc2_art_aids_deaths=hc2,
    )

    result = extract_aids_deaths_on_treatment(output)

    assert result.shape == (n_year, n_sex, 2 + 3 + 4)
    np.testing.assert_allclose(result[..., :2], 0.25 * 7 * 3)
    np.testing.assert_allclose(result[..., 2:5], 0.1 * 6 * 3)
    np.testing.assert_allclose(result[..., 5:], 0.5 * 7 * 3)


def test_registry_has_all_five_blessed_indicators():
    assert set(INDICATORS) == {
        "total_population",
        "hiv_population",
        "treatment_population",
        "aids_deaths_single_age",
        "aids_deaths_on_treatment",
    }


def test_every_indicator_shares_the_common_registry_shape():
    for spec in INDICATORS.values():
        assert callable(spec["extract"])
        assert isinstance(spec["tolerance"], Tolerance)
        assert isinstance(spec["exclusions"], tuple)
        assert isinstance(spec["scope"], Scope)


def test_rtol_is_uniform_across_indicators():
    rtols = {spec["tolerance"].rtol for spec in INDICATORS.values()}
    assert len(rtols) == 1


def test_all_five_blessed_indicators_default_to_scope_all():
    """Ticket 20's Answer: the blessed five keep today's unscoped behavior."""
    assert all(spec["scope"] is ALL for spec in INDICATORS.values())


class TestScope:
    """Ticket 20's PJNZ-scope selector: which tags (from `classify.classify`) an indicator requires."""

    def test_all_applies_to_any_tag_set_including_empty(self):
        assert ALL.applies_to(frozenset())
        assert ALL.applies_to(frozenset({"has_pmtct", "goals"}))

    def test_tag_scoped_indicator_is_skipped_for_a_pjnz_without_the_tag(self):
        scope = tag("has_pmtct")

        assert not scope.applies_to(frozenset())
        assert not scope.applies_to(frozenset({"has_cotrim", "aim_only"}))

    def test_tag_scoped_indicator_runs_for_a_pjnz_with_the_tag(self):
        scope = tag("has_pmtct")

        assert scope.applies_to(frozenset({"has_pmtct"}))
        assert scope.applies_to(frozenset({"has_pmtct", "aim_only"}))

    def test_tag_with_multiple_names_requires_every_tag_present(self):
        scope = tag("has_pmtct", "has_cotrim")

        assert not scope.applies_to(frozenset({"has_pmtct"}))
        assert scope.applies_to(frozenset({"has_pmtct", "has_cotrim"}))

    def test_tag_requires_at_least_one_name(self):
        with pytest.raises(ValueError, match="at least one"):
            tag()

    def test_scope_is_frozen_and_comparable(self):
        assert tag("has_pmtct") == tag("has_pmtct")
        assert tag("has_pmtct") != tag("has_cotrim")
