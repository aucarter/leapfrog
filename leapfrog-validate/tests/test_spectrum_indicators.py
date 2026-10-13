"""Tests for the tag-scoped `pmtct_need`/`cotrim_need` entries in `SPECTRUM_INDICATORS` (ticket 23).

Uses the real avenir importer against this repo's public BWA fixture (the
one PJNZ the old hardcoded `test-child-model.R` comparison covered) and a
synthetic leapfrog `output.h5` built from that PJNZ's own Spectrum values,
so no R build is needed. The real leapfrog-vs-Spectrum verdicts live in
`test_spectrum_extract.py`'s `@requires_r` tests.
"""

import importlib.util
from pathlib import Path

import h5py
import numpy as np
import pytest

from leapfrog_validate import git_utils
from leapfrog_validate.exclusions import Exclusion
from leapfrog_validate.indicators import ALL, tag
from leapfrog_validate.spectrum_extract import extract_spectrum_cotrim_need, extract_spectrum_pmtct_need
from leapfrog_validate.spectrum_indicators import SPECTRUM_INDICATORS, diff_spectrum_indicator

requires_avenir = pytest.mark.skipif(
    importlib.util.find_spec("Tools") is None or importlib.util.find_spec("SpectrumCommon") is None,
    reason="avenir-spectrum-common/avenir-spectrum-import-pjnz are not importable",
)

REPO_ROOT = git_utils.find_repo_root(Path(__file__).parent)
BWA_NUMPMTCT_PJNZ = REPO_ROOT / "leapfrogr" / "inst" / "pjnz" / "bwa_aim-no-special-elig-numpmtct.PJNZ"


def test_pmtct_need_is_scoped_to_has_pmtct_pjnz():
    assert SPECTRUM_INDICATORS["pmtct_need"]["scope"] == tag("has_pmtct")


def test_cotrim_need_is_scoped_to_has_cotrim_pjnz():
    assert SPECTRUM_INDICATORS["cotrim_need"]["scope"] == tag("has_cotrim")


def test_the_five_blessed_indicators_stay_unscoped():
    blessed = set(SPECTRUM_INDICATORS) - {"pmtct_need", "cotrim_need"}
    assert len(blessed) == 5
    assert all(SPECTRUM_INDICATORS[name]["scope"] is ALL for name in blessed)


def test_the_new_entries_share_the_tracks_uniform_rtol():
    rtols = {spec["tolerance"].rtol for spec in SPECTRUM_INDICATORS.values()}
    assert len(rtols) == 1


def _write_output(path: Path, **datasets: np.ndarray) -> Path:
    with h5py.File(path, "w") as f:
        for name, array in datasets.items():
            f.create_dataset(name, data=array)
    return path


@requires_avenir
def test_pmtct_need_passes_when_leapfrog_equals_spectrum(tmp_path):
    output = _write_output(tmp_path / "o.h5", hiv_births=extract_spectrum_pmtct_need(BWA_NUMPMTCT_PJNZ))
    verdict = diff_spectrum_indicator("pmtct_need", output, BWA_NUMPMTCT_PJNZ)
    assert verdict.passed
    assert verdict.n_cells == 61


@requires_avenir
def test_pmtct_need_fails_when_leapfrog_is_far_from_spectrum(tmp_path):
    output = _write_output(tmp_path / "o.h5", hiv_births=extract_spectrum_pmtct_need(BWA_NUMPMTCT_PJNZ) * 1.5)
    verdict = diff_spectrum_indicator("pmtct_need", output, BWA_NUMPMTCT_PJNZ)
    assert not verdict.passed


@requires_avenir
def test_cotrim_need_passes_when_leapfrog_equals_spectrum(tmp_path):
    output = _write_output(tmp_path / "o.h5", cotrim_need=extract_spectrum_cotrim_need(BWA_NUMPMTCT_PJNZ))
    verdict = diff_spectrum_indicator("cotrim_need", output, BWA_NUMPMTCT_PJNZ)
    assert verdict.passed


@requires_avenir
def test_cotrim_need_fails_when_leapfrog_is_far_from_spectrum(tmp_path):
    output = _write_output(tmp_path / "o.h5", cotrim_need=extract_spectrum_cotrim_need(BWA_NUMPMTCT_PJNZ) * 1.5)
    verdict = diff_spectrum_indicator("cotrim_need", output, BWA_NUMPMTCT_PJNZ)
    assert not verdict.passed


@requires_avenir
def test_year_only_indicators_accept_a_pjnz_label_for_exclusion_matching(tmp_path, monkeypatch):
    """`spectrum_track` always passes `pjnz_label`, so a 1-D indicator must work with it."""
    wrong = extract_spectrum_cotrim_need(BWA_NUMPMTCT_PJNZ) * 1.5
    output = _write_output(tmp_path / "o.h5", cotrim_need=wrong)
    spec = dict(SPECTRUM_INDICATORS["cotrim_need"])
    spec["exclusions"] = (Exclusion(pjnz="bwa.PJNZ", reason="known gap", link="https://example.com/issue/1"),)
    monkeypatch.setitem(SPECTRUM_INDICATORS, "cotrim_need", spec)

    unmatched = diff_spectrum_indicator("cotrim_need", output, BWA_NUMPMTCT_PJNZ, pjnz_label="other.PJNZ")
    matched = diff_spectrum_indicator("cotrim_need", output, BWA_NUMPMTCT_PJNZ, pjnz_label="bwa.PJNZ")

    assert not unmatched.passed
    assert matched.passed
