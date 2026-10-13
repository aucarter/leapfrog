"""Tests for the regression-track sticky-comment formatter (ticket 21).

Per the PRD's Testing Decisions, the reporting layer's test seam is exactly
this: known-input `Verdict`-shaped data in, exact rendered markdown out --
no R build, no real corpus, no live GitHub API. `render_comment` is a pure
function (`RegressionReport` in, markdown string out); everything here
drives it directly with synthetic data.

The other half of `report_regression.py` (`compute_regression_report`,
which actually runs build/run/diff) is tested separately in
`test_report_regression_compute.py` with the same build/run mocking
pattern `test_cli_compare.py` already established.
"""

import re

from leapfrog_validate.report_regression import (
    PjnzRegressionResult,
    RegressionReport,
    ResolvedRef,
    render_comment,
)

REF = ResolvedRef(given="a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2", sha="a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2")
CANDIDATE = ResolvedRef(given="main", sha="deadbeefdeadbeefdeadbeefdeadbeefdeadbeef")


def _report(results: tuple[PjnzRegressionResult, ...]) -> RegressionReport:
    return RegressionReport(ref=REF, candidate=CANDIDATE, results=results)


class TestPjnzRegressionResult:
    def test_passed_when_all_applicable_indicators_passed(self):
        result = PjnzRegressionResult(label="a.PJNZ", n_passed=5, n_applicable=5)
        assert result.passed is True

    def test_not_passed_when_any_applicable_indicator_failed(self):
        result = PjnzRegressionResult(label="a.PJNZ", n_passed=3, n_applicable=5)
        assert result.passed is False

    def test_pass_rate_is_fraction_of_indicators_passed(self):
        result = PjnzRegressionResult(label="a.PJNZ", n_passed=3, n_applicable=5)
        assert result.pass_rate == 3 / 5

    def test_no_applicable_indicators_is_a_vacuous_pass(self):
        result = PjnzRegressionResult(label="a.PJNZ", n_passed=0, n_applicable=0)
        assert result.passed is True
        assert result.pass_rate == 1.0


class TestResolvedRefDisplay:
    def test_bare_sha_displays_as_short_sha_only(self):
        sha = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2"
        ref = ResolvedRef(given=sha, sha=sha)
        assert ref.display() == "a1b2c3d4e5f6"

    def test_branch_name_displays_as_branch_name_and_short_sha(self):
        ref = ResolvedRef(given="main", sha="deadbeefdeadbeefdeadbeefdeadbeefdeadbeef")
        assert ref.display() == "main (deadbeefdead)"


class TestRegressionReportRollup:
    def test_overall_passed_true_when_every_file_passed(self):
        report = _report(
            (
                PjnzRegressionResult(label="a.PJNZ", n_passed=5, n_applicable=5),
                PjnzRegressionResult(label="b.PJNZ", n_passed=5, n_applicable=5),
            )
        )
        assert report.overall_passed is True
        assert report.overall_pass_rate == 1.0
        assert report.failed_results == ()

    def test_overall_passed_false_when_any_file_failed(self):
        report = _report(
            (
                PjnzRegressionResult(label="a.PJNZ", n_passed=5, n_applicable=5),
                PjnzRegressionResult(label="b.PJNZ", n_passed=3, n_applicable=5),
            )
        )
        assert report.overall_passed is False
        # File-level pass rate (the "headline" number) is the fraction of
        # *files* that passed overall, not the fraction of indicator
        # checks -- that finer-grained number is `pass_rate` on the
        # per-file result shown next to each failed filename instead
        # (PRD's "headline pass rate" vs. per-row "indicators-passed
        # fraction" split).
        assert report.overall_pass_rate == 1 / 2
        assert report.failed_results == (PjnzRegressionResult(label="b.PJNZ", n_passed=3, n_applicable=5),)

    def test_empty_results_is_a_vacuous_pass(self):
        report = _report(())
        assert report.overall_passed is True
        assert report.overall_pass_rate == 1.0


class TestRenderCommentAllPassing:
    def setup_method(self):
        self.report = _report(
            (
                PjnzRegressionResult(label="a.PJNZ", n_passed=5, n_applicable=5),
                PjnzRegressionResult(label="b.PJNZ", n_passed=5, n_applicable=5),
            )
        )
        self.markdown = render_comment(self.report)

    def test_titled_by_what_it_compares_not_track_a(self):
        assert "leapfrog vs leapfrog regression" in self.markdown
        assert "track a" not in self.markdown.lower()
        assert "track b" not in self.markdown.lower()

    def test_shows_a_pass_result_with_overall_percent(self):
        assert "PASS" in self.markdown
        assert "100%" in self.markdown

    def test_no_failed_files_section(self):
        assert "Failed files" not in self.markdown
        assert "All files passed" in self.markdown

    def test_never_shows_a_raw_diff_magnitude(self):
        # The data model this renders from never carries a magnitude at
        # all, but assert directly on the rendered text too, since this is
        # exactly the user-visible surface the "never leak a number" rule
        # is about.
        assert "max|delta|" not in self.markdown
        assert not re.search(r"\d+\.\d+e[+-]\d+", self.markdown)

    def test_no_plot_links(self):
        # Phase 1 has no hosted drill-down view yet (PRD's Reporting & UI).
        assert "http" not in self.markdown


class TestRenderCommentWithFailures:
    def setup_method(self):
        self.report = _report(
            (
                PjnzRegressionResult(label="good.PJNZ", n_passed=5, n_applicable=5),
                PjnzRegressionResult(label="ETH/bad.PJNZ", n_passed=3, n_applicable=5),
            )
        )
        self.markdown = render_comment(self.report)

    def test_shows_a_fail_result_with_overall_percent(self):
        # 1 of 2 files passed overall.
        assert "FAIL" in self.markdown
        assert "50%" in self.markdown

    def test_lists_the_failed_file_and_the_passing_one_in_its_own_section(self):
        assert "ETH/bad.PJNZ" in self.markdown
        # Passed files are listed too, in their own collapsible section --
        # not hidden entirely, so "100%"/"50%" etc. is backed by named files.
        assert "<summary>Files that passed (1)</summary>" in self.markdown
        assert "good.PJNZ" in self.markdown

    def test_failed_file_shows_its_own_pass_rate(self):
        assert "60%" in self.markdown
        assert "3/5" in self.markdown or "3 of 5" in self.markdown

    def test_reproduce_command_uses_resolved_shas_not_moving_ref_names(self):
        # The PRD explicitly forbids a moving ref name like "main" inside
        # the reproduce command's actual argument -- only the resolved SHA.
        # Strip the one permitted display occurrence of "main" (the
        # branch-name-plus-SHA display convention, checked separately
        # below) before asserting the bare word never appears elsewhere,
        # e.g. as a literal CLI argument.
        without_display = self.markdown.replace("main (deadbeefdead)", "")
        assert REF.sha in self.markdown
        assert CANDIDATE.sha in self.markdown
        assert "main" not in without_display

    def test_reproduce_command_is_copy_pasteable_and_mentions_the_file(self):
        assert "ETH/bad.PJNZ" in self.markdown
        assert "leapfrog-validate" in self.markdown
        assert "```" in self.markdown

    def test_branch_display_convention_shown_for_candidate(self):
        # Display-only convention: "branch-name (exact SHA)" -- doesn't
        # change the reproduce command's actual resolved-SHA argument,
        # checked separately above.
        assert "main (deadbeefdead" in self.markdown

    def test_never_shows_a_raw_diff_magnitude(self):
        assert "max|delta|" not in self.markdown
        assert not re.search(r"\d+\.\d+e[+-]\d+", self.markdown)


class TestRenderCommentEdgeCases:
    def test_no_pjnz_files_at_all_renders_without_crashing(self):
        markdown = render_comment(_report(()))
        assert "leapfrog vs leapfrog regression" in markdown
        assert "PASS" in markdown


class TestRenderCommentErroredFilesCountAsFailures:
    """An `errored` file (a build/run/classify failure) counts as a failure.

    Reverses an earlier design: if the check cannot build or run a file at
    all, that is itself a real problem worth seeing as a failure, not a
    number quietly carved out of the headline pass rate. It still shows a
    distinct reason (not an indicator pass count) so it stays visually
    distinguishable from a genuine indicator mismatch.
    """

    def setup_method(self):
        self.report = _report(
            (
                PjnzRegressionResult(label="good_a.PJNZ", n_passed=5, n_applicable=5),
                PjnzRegressionResult(label="good_b.PJNZ", n_passed=5, n_applicable=5),
                PjnzRegressionResult(
                    label="broken_a.PJNZ", n_passed=0, n_applicable=0, errored=True, error_reason="boom a"
                ),
                PjnzRegressionResult(
                    label="broken_b.PJNZ", n_passed=0, n_applicable=0, errored=True, error_reason="boom b"
                ),
                PjnzRegressionResult(
                    label="broken_c.PJNZ", n_passed=0, n_applicable=0, errored=True, error_reason="boom c"
                ),
            )
        )
        self.markdown = render_comment(self.report)

    def test_errored_files_count_toward_the_headline_pass_rate(self):
        # 2 of 5 files passed -- the 3 errored files count as failures.
        assert self.report.overall_pass_rate == 2 / 5
        assert self.report.overall_passed is False
        assert "40% of files pass (2/5)" in self.markdown
        assert "FAIL" in self.markdown

    def test_errored_files_are_listed_under_failed_files_with_their_reason(self):
        assert "### Failed files" in self.markdown
        assert "broken_a.PJNZ" in self.markdown
        assert "boom a" in self.markdown
        assert "broken_b.PJNZ" in self.markdown
        assert "broken_c.PJNZ" in self.markdown

    def test_errored_result_properties(self):
        assert len(self.report.errored_results) == 3
        assert len(self.report.failed_results) == 3
        assert len(self.report.passed_results) == 2


class TestRenderCommentGenuineFailureStillReported:
    """A real indicator mismatch must still show up as a genuine failure, distinct from errors."""

    def setup_method(self):
        self.report = _report(
            (
                PjnzRegressionResult(label="regressed.PJNZ", n_passed=2, n_applicable=5),
                PjnzRegressionResult(
                    label="broken.PJNZ", n_passed=0, n_applicable=0, errored=True, error_reason="synthetic failure"
                ),
            )
        )
        self.markdown = render_comment(self.report)

    def test_overall_result_is_fail_and_both_files_count(self):
        # 0 of 2 files passed -- both the regression and the errored file count.
        assert self.report.overall_pass_rate == 0.0
        assert self.report.overall_passed is False
        assert "FAIL" in self.markdown
        assert "0% of files pass (0/2)" in self.markdown

    def test_both_failures_present_and_distinguishable(self):
        assert "### Failed files" in self.markdown
        assert "regressed.PJNZ" in self.markdown
        assert "40%" in self.markdown  # 2/5 indicators
        assert "broken.PJNZ" in self.markdown
        assert "could not be compared: synthetic failure" in self.markdown
