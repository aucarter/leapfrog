"""Tests for `compute_regression_report`, the non-printing compare pipeline (ticket 21).

Mirrors `test_cli_compare.py`'s mocking pattern (`cli._prepare_ref`,
`cli.params.build_params`, `cli.model_run.run_model` faked out) so these run
without R -- `compute_regression_report`'s own job is reusing `cli`'s
per-PJNZ build/run/diff helpers and rolling the result up into
`PjnzRegressionResult`s without ever printing (or returning) a raw diff
magnitude; it isn't re-proving that build/run/diff pipeline itself, which
`test_cli_integration.py` already covers end-to-end against real refs.

Also asserts directly on what gets logged: only progress text, PASS/FAIL,
and filenames -- never a number that could be a diff magnitude or an
indicator value, per ticket 21's "CI logs never print actual diff values"
requirement. That requirement is about *this* log callable specifically
(stdout in CI); the rendered comment markdown is checked separately in
`test_report_regression.py`.
"""

import hashlib
import re
from pathlib import Path

import h5py
import numpy as np
import pytest

from leapfrog_validate import classify, cli, git_utils
from leapfrog_validate.build import BuildWorkspace
from leapfrog_validate.report_regression import (
    STICKY_COMMENT_HEADER,
    RegressionTrackError,
    compute_regression_report,
    main,
)


def _write_output_h5(path: Path, scale: float) -> None:
    n_year, n_sex = 2, 2
    hc1_age, hc2_age, h_age = 2, 3, 4
    total_age = hc1_age + hc2_age + h_age

    with h5py.File(path, "w") as f:
        f.create_dataset("p_totpop", data=np.full((n_year, n_sex, total_age), 100.0 * scale))
        f.create_dataset("p_hivpop", data=np.full((n_year, n_sex, total_age), 10.0 * scale))
        f.create_dataset("p_hiv_deaths", data=np.full((n_year, n_sex, total_age), 1.0 * scale))
        f.create_dataset("h_artpop", data=np.full((n_year, n_sex, h_age, 7, 3), 1.0 * scale))
        f.create_dataset("hc1_artpop", data=np.full((n_year, n_sex, hc1_age, 7, 3), 1.0 * scale))
        f.create_dataset("hc2_artpop", data=np.full((n_year, n_sex, hc2_age, 6, 3), 1.0 * scale))
        f.create_dataset("h_hiv_deaths_art", data=np.full((n_year, n_sex, h_age, 7, 3), 0.1 * scale))
        f.create_dataset("hc1_art_aids_deaths", data=np.full((n_year, n_sex, hc1_age, 7, 3), 0.1 * scale))
        f.create_dataset("hc2_art_aids_deaths", data=np.full((n_year, n_sex, hc2_age, 6, 3), 0.1 * scale))


def _fake_sha(ref: str) -> str:
    """Return a deterministic, real-looking (40 hex chars) stand-in SHA for a given ref string."""
    return hashlib.sha256(ref.encode()).hexdigest()[:40]


@pytest.fixture(autouse=True)
def fake_git_resolve(monkeypatch):
    """`git_utils.resolve_ref` returns a deterministic fake SHA per ref, no real git involved.

    Autouse (every test in this module needs it; none assert on it
    directly) -- `compute_regression_report` always resolves `ref`/
    `candidate` via `git_utils.resolve_ref` before touching `_prepare_ref`,
    so leaving this unpatched would make every test depend on real git
    state (e.g. whether a local `main`/`candidate-branch` ref exists).
    """

    def fake_resolve_ref(repo_root: Path, ref: str) -> str:
        del repo_root
        return _fake_sha(ref)

    monkeypatch.setattr(git_utils, "resolve_ref", fake_resolve_ref)


@pytest.fixture
def fake_build_and_run(monkeypatch):
    """Fake `_prepare_ref`/`build_params`/`run_model`, same shape as `test_cli_compare.py`'s fixture."""

    def fake_prepare_ref(repo_root: Path, cache_dir: Path, ref: str) -> BuildWorkspace:
        del repo_root, cache_dir
        worktree = Path(f"/fake/{ref}")
        return BuildWorkspace(worktree=worktree, r_library=worktree / "r-library")

    def fake_build_params(workspace: BuildWorkspace, pjnz: Path, output: Path) -> None:
        del workspace, pjnz, output

    def fake_run_model(workspace: BuildWorkspace, params_path: Path, output: Path, configuration: str) -> None:
        del configuration
        side = "candidate" if params_path.stem.endswith("-candidate-params") else "ref"
        pjnz_stem = params_path.stem.removesuffix(f"-{side}-params")
        del workspace
        ref_scale, candidate_scale = scales[pjnz_stem]
        scale = candidate_scale if side == "candidate" else ref_scale
        _write_output_h5(output, scale=scale)

    scales: dict[str, tuple[float, float]] = {}
    monkeypatch.setattr(cli, "_prepare_ref", fake_prepare_ref)
    monkeypatch.setattr(cli.params, "build_params", fake_build_params)
    monkeypatch.setattr(cli.model_run, "run_model", fake_run_model)
    return scales


def _make_corpus(tmp_path: Path, *names: str) -> Path:
    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    for name in names:
        (pjnz_dir / name).touch()
    return pjnz_dir


class TestComputeRegressionReportPassFail:
    def test_all_matching_files_pass(self, fake_build_and_run, tmp_path):
        fake_build_and_run["a"] = (1.0, 1.0)
        fake_build_and_run["b"] = (1.0, 1.0)
        pjnz_dir = _make_corpus(tmp_path, "a.PJNZ", "b.PJNZ")

        report = compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
        )

        assert report.overall_passed is True
        labels = {r.label: r for r in report.results}
        assert labels["a.PJNZ"].passed is True
        assert labels["b.PJNZ"].passed is True
        assert labels["a.PJNZ"].n_applicable == 5

    def test_diverging_file_fails_only_that_file(self, fake_build_and_run, tmp_path):
        fake_build_and_run["matching"] = (1.0, 1.0)
        fake_build_and_run["diverging"] = (1.0, 1.5)
        pjnz_dir = _make_corpus(tmp_path, "matching.PJNZ", "diverging.PJNZ")

        report = compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
        )

        assert report.overall_passed is False
        labels = {r.label: r for r in report.results}
        assert labels["matching.PJNZ"].passed is True
        assert labels["diverging.PJNZ"].passed is False
        assert labels["diverging.PJNZ"].n_passed < labels["diverging.PJNZ"].n_applicable


class TestComputeRegressionReportRefResolution:
    def test_resolved_shas_come_from_git_utils_resolve_ref(self, fake_build_and_run, tmp_path):
        fake_build_and_run["a"] = (1.0, 1.0)
        pjnz_dir = _make_corpus(tmp_path, "a.PJNZ")

        report = compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="abc123",
            candidate="main",
            pjnz_dir=pjnz_dir,
        )

        assert report.ref.given == "abc123"
        assert report.ref.sha == _fake_sha("abc123")
        assert report.candidate.given == "main"
        assert report.candidate.sha == _fake_sha("main")


class TestComputeRegressionReportCorpusSourceValidation:
    def test_requires_exactly_one_corpus_source(self, tmp_path):
        with pytest.raises(RegressionTrackError, match="exactly one"):
            compute_regression_report(
                repo_root=tmp_path,
                cache_dir=tmp_path / "cache",
                ref="pr-head",
                candidate="main",
            )

    def test_rejects_both_corpus_sources_given_together(self, tmp_path):
        pjnz_dir = _make_corpus(tmp_path, "a.PJNZ")
        with pytest.raises(RegressionTrackError, match="exactly one"):
            compute_regression_report(
                repo_root=tmp_path,
                cache_dir=tmp_path / "cache",
                ref="pr-head",
                candidate="main",
                pjnz_dir=pjnz_dir,
                azure_container="corpus",
                azure_account="acct",
            )

    def test_requires_azure_account_with_azure_container(self, tmp_path):
        with pytest.raises(RegressionTrackError, match="azure_account"):
            compute_regression_report(
                repo_root=tmp_path,
                cache_dir=tmp_path / "cache",
                ref="pr-head",
                candidate="main",
                azure_container="corpus",
            )

    def test_empty_pjnz_dir_raises(self, tmp_path):
        empty_dir = tmp_path / "empty"
        empty_dir.mkdir()
        with pytest.raises(RegressionTrackError, match="No PJNZ files found"):
            compute_regression_report(
                repo_root=tmp_path,
                cache_dir=tmp_path / "cache",
                ref="pr-head",
                candidate="main",
                pjnz_dir=empty_dir,
            )


class TestComputeRegressionReportClassifyErrorHandling:
    def test_classify_error_on_one_file_does_not_abort_the_whole_batch(self, fake_build_and_run, tmp_path, monkeypatch):
        fake_build_and_run["good"] = (1.0, 1.0)
        fake_build_and_run["bad"] = (1.0, 1.0)
        pjnz_dir = _make_corpus(tmp_path, "good.PJNZ", "bad.PJNZ")
        manifest_path = tmp_path / "manifest.json"
        manifest_path.write_text("{}")  # Forces needs_tags=True without needing a real tag-scoped indicator.

        def fake_classify(workspace, pjnz, manifest_data, corpus_root=None):
            del workspace, manifest_data, corpus_root
            if pjnz.stem == "bad":
                msg = "synthetic failure"
                raise classify.ClassifyError(msg)
            return frozenset()

        monkeypatch.setattr(classify, "classify", fake_classify)
        report = compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
            manifest_path=manifest_path,
        )

        labels = {r.label: r for r in report.results}
        assert labels["good.PJNZ"].passed is True
        assert labels["bad.PJNZ"].passed is False
        assert labels["bad.PJNZ"].errored is True
        # An errored file counts as a failure, same as a genuine
        # regression -- a build/run/classify failure is itself a real
        # problem worth seeing on every PR, not a number carved out of the
        # headline. It still carries a real, summarized reason, not a
        # generic placeholder.
        assert report.overall_passed is False
        assert labels["bad.PJNZ"] in report.failed_results
        assert labels["bad.PJNZ"] in report.errored_results
        assert "synthetic failure" in labels["bad.PJNZ"].error_reason


class TestComputeRegressionReportBuildRunErrorHandling:
    """A build/run failure on one PJNZ must not abort the whole batch.

    Found live against the real private corpus during this ticket's
    build: a real file hits a pre-existing leapfrogr limitation inside
    `params.build_params` (unrelated to this ticket, same shape as ticket
    16's documented `process_pjnz_ha` issue) and, before this fix, crashed
    the whole `compute_regression_report` call with an uncaught
    `ParamsBuildError` -- losing every other file's already-computed
    result. Same fix shape as ticket 20's own `ClassifyError` handling.
    """

    def test_params_build_error_on_one_file_does_not_abort_the_whole_batch(
        self, fake_build_and_run, tmp_path, monkeypatch
    ):
        fake_build_and_run["good"] = (1.0, 1.0)
        pjnz_dir = _make_corpus(tmp_path, "good.PJNZ", "bad.PJNZ")

        real_build_params = cli.params.build_params

        def flaky_build_params(workspace, pjnz, output):
            if pjnz.stem == "bad":
                msg = "synthetic ParamsBuildError"
                raise cli.params.ParamsBuildError(msg)
            real_build_params(workspace, pjnz, output)

        monkeypatch.setattr(cli.params, "build_params", flaky_build_params)
        report = compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
        )

        labels = {r.label: r for r in report.results}
        assert labels["good.PJNZ"].passed is True
        assert labels["bad.PJNZ"].passed is False
        assert labels["bad.PJNZ"].errored is True
        assert report.overall_passed is False
        assert labels["bad.PJNZ"] in report.failed_results
        assert labels["bad.PJNZ"] in report.errored_results
        assert "synthetic ParamsBuildError" in labels["bad.PJNZ"].error_reason

    def test_model_run_error_on_one_file_does_not_abort_the_whole_batch(
        self, fake_build_and_run, tmp_path, monkeypatch
    ):
        fake_build_and_run["good"] = (1.0, 1.0)
        pjnz_dir = _make_corpus(tmp_path, "good.PJNZ", "bad.PJNZ")

        real_run_model = cli.model_run.run_model

        def flaky_run_model(workspace, params_path, output, configuration):
            if params_path.stem.removesuffix("-ref-params").removesuffix("-candidate-params") == "bad":
                msg = "synthetic ModelRunError"
                raise cli.model_run.ModelRunError(msg)
            real_run_model(workspace, params_path, output, configuration)

        monkeypatch.setattr(cli.model_run, "run_model", flaky_run_model)
        report = compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
        )

        labels = {r.label: r for r in report.results}
        assert labels["good.PJNZ"].passed is True
        assert labels["bad.PJNZ"].passed is False
        assert labels["bad.PJNZ"].errored is True
        assert report.overall_passed is False
        assert labels["bad.PJNZ"] in report.failed_results
        assert labels["bad.PJNZ"] in report.errored_results
        assert "synthetic ModelRunError" in labels["bad.PJNZ"].error_reason

    def test_build_run_error_log_line_never_includes_the_exception_message(
        self, fake_build_and_run, tmp_path, monkeypatch
    ):
        del fake_build_and_run  # only needed for its _prepare_ref mocking side effect
        pjnz_dir = _make_corpus(tmp_path, "bad.PJNZ")

        def always_fails(workspace, pjnz, output):
            del workspace, pjnz, output
            msg = "--- stderr ---\nsome subprocess output that must never be logged"
            raise cli.params.ParamsBuildError(msg)

        monkeypatch.setattr(cli.params, "build_params", always_fails)
        log_lines: list[str] = []
        compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
            log=log_lines.append,
        )

        assert any("ERROR" in line and "bad.PJNZ" in line for line in log_lines)
        assert not any("stderr" in line for line in log_lines)


class TestComputeRegressionReportLogging:
    """The `log` callable stands in for CI's own job log -- assert nothing number-shaped leaks through."""

    _NUMBER_LIKE = re.compile(r"\d+\.\d|\de[+-]\d|\d+/\d+")

    def test_log_never_receives_a_number_shaped_line(self, fake_build_and_run, tmp_path):
        fake_build_and_run["matching"] = (1.0, 1.0)
        fake_build_and_run["diverging"] = (1.0, 1.5)
        pjnz_dir = _make_corpus(tmp_path, "matching.PJNZ", "diverging.PJNZ")

        log_lines: list[str] = []
        compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
            log=log_lines.append,
        )

        assert log_lines, "expected at least one progress line"
        for line in log_lines:
            assert not self._NUMBER_LIKE.search(line), f"log line looks like it carries a number: {line!r}"

    def test_log_lines_are_only_progress_pass_fail_and_filenames(self, fake_build_and_run, tmp_path):
        fake_build_and_run["a"] = (1.0, 1.0)
        pjnz_dir = _make_corpus(tmp_path, "a.PJNZ")

        log_lines: list[str] = []
        compute_regression_report(
            repo_root=tmp_path,
            cache_dir=tmp_path / "cache",
            ref="pr-head",
            candidate="main",
            pjnz_dir=pjnz_dir,
            log=log_lines.append,
        )

        assert any("a.PJNZ" in line for line in log_lines)
        assert any(line.startswith(("PASS", "FAIL")) for line in log_lines)


class TestMainCliPrintHeader:
    """`--print-header` is a standalone early-exit mode.

    Code review: avoid a second, independently-typed copy of the
    sticky-comment header string in the workflow YAML.
    """

    def test_print_header_prints_the_constant_and_exits_zero(self, capsys):
        exit_code = main(["--print-header"])

        assert exit_code == 0
        assert capsys.readouterr().out.strip() == STICKY_COMMENT_HEADER

    def test_print_header_needs_no_other_argument(self, capsys):
        # Deliberately not passing --ref/--candidate/--out -- would be a
        # required-argument error for a normal run, but --print-header
        # short-circuits before any of those are consulted.
        exit_code = main(["--print-header"])
        assert exit_code == 0
        capsys.readouterr()

    def test_missing_ref_and_out_without_print_header_is_a_clean_error(self, capsys):
        exit_code = main([])

        assert exit_code == 2
        assert "--ref" in capsys.readouterr().err
