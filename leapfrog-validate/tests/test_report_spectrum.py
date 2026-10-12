"""Tests for the Spectrum-track sticky-comment formatter.

Pure-function tests against synthetic `PjnzResult`/`IndicatorResult` data --
no R build, no network, no real PJNZ (per the PRD's Testing Decisions: "The
reporting layer ... should be tested by asserting generated comment text
against known input verdicts"). TDD'd before `spectrum_extract.py`'s real
extraction logic, per the ticket's workflow instructions.
"""

import re

from leapfrog_validate.report_spectrum import (
    COMMENT_MARKER,
    IndicatorResult,
    PjnzResult,
    format_comment,
)


def _passing(*names: str) -> tuple[IndicatorResult, ...]:
    return tuple(IndicatorResult(name=n, passed=True) for n in names)


def test_comment_always_carries_the_hidden_marker():
    body = format_comment([], ref_given="main", ref_sha="abc1234")
    assert COMMENT_MARKER in body


def test_comment_is_titled_leapfrog_vs_spectrum_not_track_b():
    body = format_comment([], ref_given="main", ref_sha="abc1234")
    assert "leapfrog vs Spectrum comparison" in body
    assert "Track B" not in body
    assert "Track A" not in body


def test_comment_states_it_never_blocks_merging():
    body = format_comment([], ref_given="main", ref_sha="abc1234")
    assert "does not block" in body.lower() or "never block" in body.lower()


def test_no_files_compared_is_reported_plainly():
    body = format_comment([], ref_given="main", ref_sha="abc1234")
    assert "0% pass" not in body  # undefined, not a bogus 0%
    assert "No PJNZ files" in body


def test_all_passing_reports_100_percent_and_no_failed_list():
    results = [
        PjnzResult(label="a.PJNZ", indicators=_passing("total_population", "hiv_population")),
        PjnzResult(label="b.PJNZ", indicators=_passing("total_population", "hiv_population")),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "100% pass" in body
    assert "a.PJNZ" not in body
    assert "b.PJNZ" not in body


def test_a_failed_file_is_named_with_its_pass_rate():
    results = [
        PjnzResult(
            label="bwa_aim-no-special-elig-numpmtct.PJNZ",
            indicators=(
                IndicatorResult(name="total_population", passed=True),
                IndicatorResult(name="hiv_population", passed=False),
                IndicatorResult(name="treatment_population", passed=False),
                IndicatorResult(name="aids_deaths_single_age", passed=False),
                IndicatorResult(name="aids_deaths_on_treatment", passed=False),
            ),
        ),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "bwa_aim-no-special-elig-numpmtct.PJNZ" in body
    assert "1/5" in body
    assert "0% pass" in body


def test_overall_percent_pass_counts_whole_files_not_indicators():
    results = [
        PjnzResult(label="a.PJNZ", indicators=_passing("x")),
        PjnzResult(label="b.PJNZ", indicators=(IndicatorResult(name="x", passed=False),)),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "50% pass" in body


def test_an_errored_file_is_listed_separately_and_not_counted_in_percent_pass():
    """Pin that an errored file is excluded from the "% pass" denominator.

    Regression test for a real finding from ticket 22's own live
    verification: a build/run failure (a pre-existing leapfrogr
    compatibility gap, not a tolerance disagreement) must not silently
    drag down "% pass" as if it were a real divergence -- it gets its own
    "could not be compared" category instead, excluded from that rate.
    Independently, ticket 21's regression-track comment hit and fixed the
    identical conflation on its own real run.
    """
    results = [
        PjnzResult(label="passing.PJNZ", indicators=_passing("total_population")),
        PjnzResult(label="broken.PJNZ", indicators=(), error="could not build/run leapfrog for this PJNZ"),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "broken.PJNZ" in body
    assert "passing.PJNZ" not in body  # only failed/errored files are named
    # One real file was comparable, and it passed -- the errored file must
    # not be averaged into that rate as a failure.
    assert "100% pass" in body
    assert "1/1 files compared" in body
    assert "could not be compared" in body
    assert "could not build/run leapfrog for this PJNZ" in body


def test_when_every_file_errors_percent_pass_is_not_shown_as_a_bogus_number():
    results = [
        PjnzResult(label="broken.PJNZ", indicators=(), error="could not read Spectrum modvars"),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "broken.PJNZ" in body
    assert "% pass" not in body  # nothing was actually compared -- no rate to report
    assert "No files could be compared" in body
    # The underlying exception message is safe (filenames/structure only,
    # never a raw diff value), but the comment still needs to say *something*
    # happened rather than silently showing a pass-rate fraction for no data.
    assert "could not read Spectrum modvars" in body


def test_failed_file_gets_a_reproduce_command_with_the_resolved_sha():
    results = [
        PjnzResult(label="bwa.PJNZ", indicators=(IndicatorResult(name="x", passed=False),)),
    ]
    body = format_comment(results, ref_given="my-branch", ref_sha="deadbeef1234")
    assert "deadbeef1234" in body
    assert "bwa.PJNZ" in body
    assert "spectrum_track" in body or "diff-one" in body


def test_a_moving_ref_is_displayed_as_branch_name_and_exact_sha():
    body = format_comment([], ref_given="my-branch", ref_sha="deadbeef1234")
    assert "my-branch (deadbeef1234)" in body


def test_a_bare_sha_ref_is_not_shown_redundantly():
    body = format_comment([], ref_given="deadbeef1234", ref_sha="deadbeef1234")
    assert "deadbeef1234 (deadbeef1234)" not in body


def test_comment_never_contains_a_decimal_number_other_than_the_percent_summary():
    # Defends the "never print actual diff values" rule at the comment layer
    # too: nothing here should look like a raw float diff (e.g. "123.456").
    results = [
        PjnzResult(
            label="x.PJNZ",
            indicators=(IndicatorResult(name="total_population", passed=False),),
        ),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")

    # Allow integers (pass-rate fractions, percentages) but not decimals.
    assert not re.search(r"\d+\.\d+", body)
