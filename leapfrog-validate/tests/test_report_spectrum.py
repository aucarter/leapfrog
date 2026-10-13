"""Tests for the Spectrum-track sticky-comment formatter.

Pure-function tests against synthetic `PjnzResult`/`IndicatorResult` data --
no R build, no network, no real PJNZ (per the PRD's Testing Decisions: "The
reporting layer ... should be tested by asserting generated comment text
against known input verdicts").
"""

import re

from _comment_asserts import assert_nested_collapsed_failures

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


def test_failed_files_are_nested_and_collapsed():
    results = [
        PjnzResult(label="good.PJNZ", indicators=_passing("x")),
        PjnzResult(label="ETH/bad.PJNZ", indicators=(IndicatorResult(name="x", passed=False),)),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert_nested_collapsed_failures(body, "<summary>❌ Failed files (1/2)</summary>", "ETH/bad.PJNZ")
    assert "<summary><code>ETH/bad.PJNZ</code> -- 0/1 indicators passed</summary>" in body


def test_file_names_are_html_escaped_inside_summaries():
    results = [PjnzResult(label="a&b<c>.PJNZ", indicators=(IndicatorResult(name="x", passed=False),))]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "<code>a&amp;b&lt;c&gt;.PJNZ</code>" in body


def test_all_passing_reports_100_percent_with_counts_and_lists_them_as_succeeded():
    results = [
        PjnzResult(label="a.PJNZ", indicators=_passing("total_population", "hiv_population")),
        PjnzResult(label="b.PJNZ", indicators=_passing("total_population", "hiv_population")),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "✅ PASS -- 100% of files pass (2/2)" in body
    assert "Every file passed." in body
    # Succeeded files are listed in a collapsible section, not hidden entirely.
    assert "<details>" in body
    assert "Succeeded files (2/2)" in body
    assert "a.PJNZ" in body
    assert "b.PJNZ" in body


def test_succeeded_files_section_is_collapsed_and_comes_before_failed_files():
    results = [
        PjnzResult(label="good.PJNZ", indicators=_passing("x")),
        PjnzResult(label="bad.PJNZ", indicators=(IndicatorResult(name="x", passed=False),)),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    succeeded_pos = body.index("<summary>✅ Succeeded files")
    failed_pos = body.index("<summary>❌ Failed files")
    assert succeeded_pos < failed_pos
    assert "<summary>✅ Succeeded files (1/2)</summary>" in body


def test_failed_entry_shows_a_tick_or_cross_per_indicator():
    results = [
        PjnzResult(
            label="mixed.PJNZ",
            indicators=(
                IndicatorResult(name="total_population", passed=True),
                IndicatorResult(name="hiv_population", passed=False),
            ),
        ),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "✅ total_population" in body
    assert "❌ hiv_population" in body


def test_lists_the_indicators_checked_with_their_tolerance():
    results = [PjnzResult(label="a.PJNZ", indicators=_passing("total_population"))]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "Indicators checked (5)" in body
    assert "total_population" in body
    assert "hiv_population" in body
    assert "treatment_population" in body
    assert "aids_deaths_single_age" in body
    assert "aids_deaths_on_treatment" in body
    # Specific tolerance values, not just the word "tolerance".
    assert "50" in body
    assert "20" in body
    assert "1%" in body


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
    assert "1/5 indicators passed" in body
    assert "0% of files pass (0/1)" in body


def test_overall_percent_pass_counts_whole_files_not_indicators():
    results = [
        PjnzResult(label="a.PJNZ", indicators=_passing("x")),
        PjnzResult(label="b.PJNZ", indicators=(IndicatorResult(name="x", passed=False),)),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "50% of files pass (1/2)" in body


def test_an_errored_file_counts_as_a_failure_and_shows_its_reason():
    """A build/run/classify failure must count toward the fail rate, not be excluded.

    Reverses an earlier design (where an "errored" file was excluded from
    the "% pass" denominator): a file the check cannot even build or run is
    itself a real problem, not a reason to quietly shrink the denominator.
    It still gets its own distinct wording (a reason, not an indicator
    count) so a build/run failure stays visually distinguishable from a
    genuine tolerance mismatch.
    """
    results = [
        PjnzResult(label="passing.PJNZ", indicators=_passing("total_population")),
        PjnzResult(label="broken.PJNZ", indicators=(), error="could not build/run leapfrog for this PJNZ (xyz)"),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "broken.PJNZ" in body
    assert "passing.PJNZ" in body
    # The errored file counts as a failure: 1 of 2 files passed.
    assert "50% of files pass (1/2)" in body
    assert "-- could not be compared</summary>" in body
    assert "Could not be compared: could not build/run leapfrog for this PJNZ (xyz)" in body


def test_error_reason_is_shown_not_just_a_generic_placeholder():
    """A maintainer needs the real failure reason to recreate it locally, not a generic label."""
    results = [
        PjnzResult(label="broken.PJNZ", indicators=(), error="'length = 9' in coercion to 'logical(1)'"),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "'length = 9' in coercion to 'logical(1)'" in body


def test_when_every_file_errors_it_is_a_complete_failure_not_a_bogus_rate():
    results = [
        PjnzResult(label="broken.PJNZ", indicators=(), error="could not read Spectrum modvars"),
    ]
    body = format_comment(results, ref_given="main", ref_sha="abc1234")
    assert "broken.PJNZ" in body
    assert "0% of files pass (0/1)" in body
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
