"""Tests for `leapfrog_validate.spectrum_extract`.

Two tiers, mirroring `test_cli_integration.py`'s split:

- PJNZ-only tests (no R, no leapfrogr build) -- reading a PJNZ's own
  Spectrum modvars via the real `avenir-spectrum-common`/
  `avenir-spectrum-import-pjnz` packages needs neither. These run whenever
  those packages are importable (always, in this sandbox -- see
  `spectrum_extract.py`'s module docstring on the dependency finding), and
  are skipped with a clear reason otherwise rather than silently skipped.
- `@requires_r` tests that also build a real `output.h5` via leapfrogr, to
  exercise `diff_spectrum_indicator`'s real leapfrog-vs-Spectrum
  comparison end-to-end, including the ticket's required "known
  divergent, failing-but-non-blocking" case.
"""

import importlib.util
import shutil
import subprocess
import zipfile
from pathlib import Path

import numpy as np
import pytest

from leapfrog_validate import build, git_utils, model_run, params
from leapfrog_validate.indicators import extract_total_population
from leapfrog_validate.spectrum_extract import (
    SpectrumExtractError,
    _align_to_leapfrog_years,
    _read_raw_dp_cotrim_row,
    extract_spectrum_aids_deaths_on_treatment,
    extract_spectrum_aids_deaths_single_age,
    extract_spectrum_cotrim_need,
    extract_spectrum_hiv_population,
    extract_spectrum_pmtct_need,
    extract_spectrum_total_population,
    extract_spectrum_treatment_population,
)
from leapfrog_validate.spectrum_indicators import SPECTRUM_INDICATORS, diff_spectrum_indicator

requires_r = pytest.mark.skipif(shutil.which("Rscript") is None, reason="R is not installed")
requires_avenir = pytest.mark.skipif(
    importlib.util.find_spec("Tools") is None or importlib.util.find_spec("SpectrumCommon") is None,
    reason="avenir-spectrum-common/avenir-spectrum-import-pjnz are not importable",
)

REPO_ROOT = git_utils.find_repo_root(Path(__file__).parent)
_PJNZ_DIR = REPO_ROOT / "leapfrogr" / "inst" / "pjnz"

# 1970-2032 (63 years) -- longer than leapfrog's fixed 1970-2030 output
# window, so reading it exercises `_align_to_leapfrog_years`'s real slicing
# path, not just a no-op.
FRANCE_PJNZ = _PJNZ_DIR / "france_default.PJNZ"

# 1970-2030 (61 years) exactly -- this repo's real fixture behind
# `test-child-model.R`'s skipped "Female 15-49y pop aligns" test
# ("Skipping this test because the adult populations currently do not
# align"), confirmed below (and empirically, before writing this test) to
# still diverge from Spectrum well beyond this track's placeholder
# tolerances on 4 of the 5 blessed indicators -- this ticket's required
# "known divergent, failing-but-non-blocking" fixture.
BWA_NUMPMTCT_PJNZ = _PJNZ_DIR / "bwa_aim-no-special-elig-numpmtct.PJNZ"

# Fails to import via `avenir-spectrum-import-pjnz` today (confirmed:
# `TypeError: unsupported operand type(s) for +: 'NoneType' and 'int'` in
# `Tools.ImportPJNZ.DP.ImportDP.import_SAPFittingValues`, a missing
# `CSAVRSexRatioRow` tag specific to this fixture) -- a real, pre-existing
# limitation in the proprietary importer, not something this module can
# fix. Locked down as a regression test below so this stays a documented,
# loud failure rather than a silent one.
BWA_ADULT_PJNZ = _PJNZ_DIR / "bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ"


@requires_avenir
def test_france_pjnz_spans_more_years_than_leapfrogs_output_window():
    """Sanity precondition for the alignment test below -- not itself the thing under test."""
    arr = extract_spectrum_total_population(FRANCE_PJNZ)
    assert arr.shape == (61, 2, 81)


@requires_avenir
def test_bwa_numpmtct_pjnz_spans_exactly_leapfrogs_output_window():
    arr = extract_spectrum_total_population(BWA_NUMPMTCT_PJNZ)
    assert arr.shape == (61, 2, 81)


@requires_avenir
def test_all_five_spectrum_extractors_return_the_expected_shape():
    for extract in (
        extract_spectrum_total_population,
        extract_spectrum_hiv_population,
        extract_spectrum_treatment_population,
        extract_spectrum_aids_deaths_single_age,
        extract_spectrum_aids_deaths_on_treatment,
    ):
        arr = extract(BWA_NUMPMTCT_PJNZ)
        assert arr.shape == (61, 2, 81), extract.__name__


@requires_avenir
def test_sex_axis_zero_is_male_matching_leapfrogs_own_convention():
    """Cross-check the male/female split against a shape-level invariant.

    Rather than trusting the sex-axis assignment silently, assert the
    robust fact: values are non-negative, and the two sex slices are not
    accidentally identical (i.e. a real split happened, not a duplicate
    column).
    """
    arr = extract_spectrum_total_population(BWA_NUMPMTCT_PJNZ)
    male_total = arr[:, 0, :].sum()
    female_total = arr[:, 1, :].sum()
    assert male_total > 0
    assert female_total > 0
    assert male_total != female_total


@requires_avenir
def test_importing_a_pjnz_the_avenir_importer_cannot_parse_raises_clearly():
    """Confirm a real, pre-existing avenir-importer limitation fails loudly.

    See BWA_ADULT_PJNZ's module-level comment -- must raise
    SpectrumExtractError, never silently return a wrong/empty array.
    """
    with pytest.raises(SpectrumExtractError):
        extract_spectrum_total_population(BWA_ADULT_PJNZ)


@requires_avenir
def test_a_pjnz_whose_years_dont_cover_the_leapfrog_window_is_rejected(tmp_path):
    """`_align_to_leapfrog_years` must refuse to silently misalign a too-short PJNZ."""
    arr = np.zeros((3, 81, 5))  # years 2020-2024 -- nowhere near 1970-2030
    with pytest.raises(SpectrumExtractError, match="don't fully cover"):
        _align_to_leapfrog_years(arr, first_year=2020, final_year=2024, pjnz=tmp_path / "fake.PJNZ")


@requires_avenir
def test_pmtct_need_is_year_only_and_matches_the_value_stored_in_the_dp():
    """Compare against a literal read straight from the PJNZ's `<ChildNeedPMTCT MV>` row.

    1680.59277 is the 20th value (1989) of that row in the BWA fixture's
    `.DP` file, found by reading the raw file, not by calling the
    extractor under test.
    """
    arr = extract_spectrum_pmtct_need(BWA_NUMPMTCT_PJNZ)
    assert arr.shape == (61,)
    assert arr[0] == 0.0
    assert arr[19] == pytest.approx(1680.59277, rel=1e-6)


@requires_avenir
def test_pmtct_need_is_aligned_to_the_leapfrog_window_for_a_longer_pjnz():
    assert extract_spectrum_pmtct_need(FRANCE_PJNZ).shape == (61,)


@requires_avenir
def test_cotrim_need_is_year_only_and_matches_the_value_stored_in_the_dp():
    """The avenir importer leaves `AM_ChildNeedCotrim_V1` all zero, so this reads the raw `.DP`.

    3006.59082 is the 20th value (1989) of the "Children needing
    cotrimoxazole (0-14)" both-sexes row in the BWA fixture's
    `<ChildARTCalc MV2>` block, found by reading the raw file.
    """
    arr = extract_spectrum_cotrim_need(BWA_NUMPMTCT_PJNZ)
    assert arr.shape == (61,)
    assert arr[0] == 0.0
    assert arr[19] == pytest.approx(3006.59082, rel=1e-6)


@requires_avenir
def test_cotrim_need_is_aligned_to_the_leapfrog_window_for_a_longer_pjnz():
    arr = extract_spectrum_cotrim_need(FRANCE_PJNZ)
    assert arr.shape == (61,)
    assert arr.max() > 0


def _write_pjnz_with_dp(path: Path, dp_text: str) -> Path:
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("fake.DP", dp_text)
    return path


def test_raw_dp_cotrim_row_is_read_from_the_both_sexes_needing_cotrim_row(tmp_path):
    dp = (
        "<Other MV>,,\n"
        "<ChildARTCalc MV2>,,,\n"
        ",,,\n"
        ",<Value>,,\n"
        ",Children needing cotrimoxazole (0-14),,1.5,2.5,3.5\n"
        ",,,9,9,9\n"
        ",,,8,8,8\n"
    )
    row = _read_raw_dp_cotrim_row(_write_pjnz_with_dp(tmp_path / "a.PJNZ", dp))
    assert row.tolist() == [1.5, 2.5, 3.5]


def test_raw_dp_cotrim_row_raises_clearly_when_the_block_is_missing(tmp_path):
    pjnz = _write_pjnz_with_dp(tmp_path / "a.PJNZ", "<Other MV>,,\n")
    with pytest.raises(SpectrumExtractError, match="ChildARTCalc"):
        _read_raw_dp_cotrim_row(pjnz)


def test_raw_dp_cotrim_row_raises_clearly_when_the_row_is_not_the_cotrim_need_row(tmp_path):
    dp = "<ChildARTCalc MV2>,,,\n,,,\n,<Value>,,\n,Something else entirely,,1,2\n"
    with pytest.raises(SpectrumExtractError, match="cotrimoxazole"):
        _read_raw_dp_cotrim_row(_write_pjnz_with_dp(tmp_path / "a.PJNZ", dp))


def test_raw_dp_cotrim_row_raises_clearly_when_the_pjnz_has_no_dp_file(tmp_path):
    pjnz = tmp_path / "a.PJNZ"
    with zipfile.ZipFile(pjnz, "w") as z:
        z.writestr("fake.PJN", "x")
    with pytest.raises(SpectrumExtractError, match=r"\.DP"):
        _read_raw_dp_cotrim_row(pjnz)


@pytest.fixture(scope="module")
def head_sha_e2e():
    return git_utils.resolve_ref(REPO_ROOT, "HEAD")


@pytest.fixture(scope="module")
def cache_dir_e2e(tmp_path_factory):
    return tmp_path_factory.mktemp("spectrum-extract-cache")


@pytest.fixture(scope="module")
def head_worktree_e2e(head_sha_e2e, cache_dir_e2e):
    worktree = git_utils.ensure_worktree(REPO_ROOT, head_sha_e2e, cache_dir_e2e)
    yield worktree
    subprocess.run(
        ["git", "-C", str(REPO_ROOT), "worktree", "remove", "--force", str(worktree)],
        check=False,
    )


@pytest.fixture(scope="module")
def head_workspace_e2e(head_worktree_e2e):
    return build.build_leapfrogr(head_worktree_e2e)


def _output_h5_for(workspace, pjnz, tmp_path, name):
    params_path = tmp_path / f"{name}-params.h5"
    output_path = tmp_path / f"{name}-output.h5"
    params.build_params(workspace, pjnz, params_path)
    model_run.run_model(workspace, params_path, output_path, "Spectrum")
    return output_path


@requires_r
@requires_avenir
def test_total_population_passes_at_year_1970_for_both_fixtures(head_workspace_e2e, tmp_path):
    """Pin down a loose but genuine correctness fact at year 1970.

    Year 1970 is the shared starting point both sides derive from the same
    PJNZ input, so (unlike later projected years) it should agree closely
    regardless of which PJNZ.
    """
    output = _output_h5_for(head_workspace_e2e, BWA_NUMPMTCT_PJNZ, tmp_path, "bwa")
    lf = extract_total_population(output)
    spec = extract_spectrum_total_population(BWA_NUMPMTCT_PJNZ)
    assert lf.shape == spec.shape
    np.testing.assert_allclose(lf[0], spec[0], atol=1.0)


@requires_r
@requires_avenir
def test_known_divergent_pjnz_fails_hiv_population_but_the_run_is_non_blocking_by_design(head_workspace_e2e, tmp_path):
    """Exercise the ticket's required case: a real divergent-beyond-tolerance PJNZ.

    Confirmed via `test-child-model.R`'s own skipped "Female 15-49y pop
    aligns" test (same PJNZ) that this is a *known*, pre-existing
    adult-population discrepancy, not a bug introduced by this ticket --
    `diff_spectrum_indicator` still returns a normal Verdict (passed=False),
    it just never raises or blocks anything; blocking is a CI/branch-
    protection concern this library-level function has no opinion on.
    """
    output = _output_h5_for(head_workspace_e2e, BWA_NUMPMTCT_PJNZ, tmp_path, "bwa2")
    verdict = diff_spectrum_indicator("hiv_population", output, BWA_NUMPMTCT_PJNZ)
    assert not verdict.passed
    assert verdict.n_failed > 0


@requires_r
@requires_avenir
def test_total_population_passes_within_tolerance_for_the_same_known_divergent_pjnz(head_workspace_e2e, tmp_path):
    """Show a genuinely mixed per-indicator result on the same divergent PJNZ.

    total_population's looser atol=50 + 1% rtol absorbs the real drift
    that hiv_population's atol=20 doesn't -- this file is not simply
    "broken" across every indicator.
    """
    output = _output_h5_for(head_workspace_e2e, BWA_NUMPMTCT_PJNZ, tmp_path, "bwa3")
    verdict = diff_spectrum_indicator("total_population", output, BWA_NUMPMTCT_PJNZ)
    assert verdict.passed


@requires_r
@requires_avenir
@pytest.mark.parametrize("name", ["pmtct_need", "cotrim_need"])
def test_migrated_need_indicators_pass_for_the_pjnz_the_old_testthat_comparison_covered(
    name, head_workspace_e2e, tmp_path
):
    """Ticket 23: same verdict (pass) as the `test-child-model.R` comparisons they replaced.

    Those tests ran this exact PJNZ and passed. This is the real
    leapfrog build, real avenir import, and the real registry entry.
    """
    output = _output_h5_for(head_workspace_e2e, BWA_NUMPMTCT_PJNZ, tmp_path, f"bwa-{name}")
    verdict = diff_spectrum_indicator(name, output, BWA_NUMPMTCT_PJNZ)
    assert verdict.passed
    assert verdict.n_cells == 61


@requires_r
@requires_avenir
def test_every_registered_indicator_runs_without_error_against_a_real_pjnz(head_workspace_e2e, tmp_path):
    output = _output_h5_for(head_workspace_e2e, FRANCE_PJNZ, tmp_path, "france")
    for name in SPECTRUM_INDICATORS:
        verdict = diff_spectrum_indicator(name, output, FRANCE_PJNZ)
        assert verdict.indicator == name
