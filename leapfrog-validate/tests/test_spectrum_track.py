"""CLI-level tests for `spectrum_track.py`'s `run`/`diff-one` commands.

Mocks the same boundary `test_cli_compare.py`/`test_cli_compare_tag_scoping.py`
use for `compare` (`_prepare_ref`, `params.build_params`, `model_run.run_model`)
plus this module's own `diff_spectrum_indicator` seam, so these tests never
need a real R build, a real PJNZ, or the avenir packages -- the real,
end-to-end-verified wiring lives in `test_spectrum_extract.py` instead.
"""

from pathlib import Path

import pytest
from typer.testing import CliRunner

from leapfrog_validate import git_utils, spectrum_track
from leapfrog_validate import model_run as model_run_module
from leapfrog_validate import params as params_module
from leapfrog_validate.build import BuildWorkspace
from leapfrog_validate.diff import Tolerance, Verdict
from leapfrog_validate.indicators import tag
from leapfrog_validate.spectrum_track import app

runner = CliRunner()


def _fake_prepare_ref(repo_root: Path, cache_dir: Path, ref: str) -> BuildWorkspace:
    del repo_root, cache_dir
    worktree = Path(f"/fake/{ref}")
    return BuildWorkspace(worktree=worktree, r_library=worktree / "r-library")


def _fake_build_params(workspace: BuildWorkspace, pjnz: Path, output: Path) -> None:
    del workspace, pjnz, output


def _fake_run_model(workspace: BuildWorkspace, params_path: Path, output: Path, configuration: str) -> None:
    del workspace, params_path, output, configuration


@pytest.fixture(autouse=True)
def fake_build_and_run(monkeypatch):
    """Give every test in this module a fast, fake build/run.

    Only `diff_spectrum_indicator` (or `diff-one`'s indicator loop) is left
    real/patched per-test, since that's the thing each test actually wants
    to exercise.
    """
    monkeypatch.setattr(spectrum_track, "_prepare_ref", _fake_prepare_ref)
    monkeypatch.setattr(params_module, "build_params", _fake_build_params)
    monkeypatch.setattr(model_run_module, "run_model", _fake_run_model)


def test_diff_one_passes_when_every_indicator_passes(monkeypatch, tmp_path):
    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        return Verdict(indicator=name, passed=True, max_abs_diff=0.0, n_cells=10, n_failed=0)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)
    pjnz = tmp_path / "a.PJNZ"
    pjnz.touch()

    result = runner.invoke(app, ["diff-one", "main", str(pjnz)])

    assert result.exit_code == 0, result.output
    assert "PASS  total_population" in result.output
    assert "FAIL" not in result.output


def test_diff_one_fails_when_any_indicator_fails(monkeypatch, tmp_path):
    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        passed = name != "hiv_population"
        return Verdict(indicator=name, passed=passed, max_abs_diff=0.0, n_cells=10, n_failed=0 if passed else 3)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)
    pjnz = tmp_path / "a.PJNZ"
    pjnz.touch()

    result = runner.invoke(app, ["diff-one", "main", str(pjnz)])

    assert result.exit_code == 1
    assert "FAIL  hiv_population" in result.output


def test_diff_one_never_prints_a_raw_diff_magnitude(monkeypatch, tmp_path):
    """Check the CI-logs requirement directly against a large Verdict magnitude.

    Even with a real, large max_abs_diff on the Verdict, diff-one's output
    must never show it.
    """

    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        return Verdict(indicator=name, passed=False, max_abs_diff=123456.789, n_cells=10, n_failed=3)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)
    pjnz = tmp_path / "a.PJNZ"
    pjnz.touch()

    result = runner.invoke(app, ["diff-one", "main", str(pjnz)])

    assert "123456" not in result.output
    assert "789" not in result.output


def test_run_requires_exactly_one_corpus_source(tmp_path):
    result = runner.invoke(app, ["run", "main", "--comment-out", str(tmp_path / "comment.md")])
    assert result.exit_code == 2
    assert "exactly one" in result.output


def test_run_pjnz_dir_is_not_broken_by_ambient_azure_env_vars(monkeypatch, tmp_path):
    """Pin the same real bug ticket 20 fixed in `compare_cmd`.

    An explicit --pjnz-dir must win regardless of ambient AZURE_STORAGE_*
    env vars (e.g. this track's own workflow exports them for its Azure
    default, but a developer running --pjnz-dir locally in that same shell
    must not have it silently rejected).
    """
    monkeypatch.setenv("AZURE_STORAGE_CONTAINER", "pjnz-archive")
    monkeypatch.setenv("AZURE_STORAGE_ACCOUNT", "someaccount")

    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        return Verdict(indicator=name, passed=True, max_abs_diff=0.0, n_cells=10, n_failed=0)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "a.PJNZ").touch()
    comment_path = tmp_path / "comment.md"

    result = runner.invoke(app, ["run", "main", "--pjnz-dir", str(pjnz_dir), "--comment-out", str(comment_path)])

    assert result.exit_code == 0, result.output


def test_run_falls_back_to_azure_env_vars_when_neither_option_is_given(monkeypatch, tmp_path):
    monkeypatch.setenv("AZURE_STORAGE_CONTAINER", "pjnz-archive")
    monkeypatch.setenv("AZURE_STORAGE_ACCOUNT", "someaccount")

    captured: dict[str, object] = {}

    def fake_resolve_corpus_files(pjnz_dir, azure_container, azure_account, azure_prefix, tmp_path_arg):  # noqa: ARG001
        captured["azure_container"] = azure_container
        captured["azure_account"] = azure_account
        return [], tmp_path_arg

    monkeypatch.setattr(spectrum_track, "_resolve_corpus_files", fake_resolve_corpus_files)

    result = runner.invoke(app, ["run", "main", "--comment-out", str(tmp_path / "comment.md")])

    assert result.exit_code == 0, result.output
    assert captured["azure_container"] == "pjnz-archive"
    assert captured["azure_account"] == "someaccount"


def test_run_rejects_both_corpus_sources_given_together(tmp_path):
    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    result = runner.invoke(
        app,
        [
            "run",
            "main",
            "--pjnz-dir",
            str(pjnz_dir),
            "--azure-container",
            "pjnz-archive",
            "--azure-account",
            "acct",
        ],
    )
    assert result.exit_code == 2
    assert "exactly one" in result.output


def test_run_writes_a_comment_file_and_reports_per_file_pass_fail(monkeypatch, tmp_path):
    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        passed = pjnz_label != "bad.PJNZ"
        return Verdict(indicator=name, passed=passed, max_abs_diff=0.0, n_cells=10, n_failed=0 if passed else 1)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "good.PJNZ").touch()
    (pjnz_dir / "bad.PJNZ").touch()
    comment_path = tmp_path / "comment.md"

    result = runner.invoke(app, ["run", "main", "--pjnz-dir", str(pjnz_dir), "--comment-out", str(comment_path)])

    assert result.exit_code == 1, result.output
    assert "PASS  good.PJNZ" in result.output
    assert "FAIL  bad.PJNZ" in result.output
    assert comment_path.exists()
    body = comment_path.read_text()
    assert "leapfrog vs Spectrum comparison" in body
    assert "bad.PJNZ" in body
    assert "good.PJNZ" in body  # passed files are listed too, in their own collapsible section


def test_run_never_prints_a_raw_diff_magnitude(monkeypatch, tmp_path):
    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        return Verdict(indicator=name, passed=False, max_abs_diff=98765.4321, n_cells=10, n_failed=5)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "a.PJNZ").touch()
    comment_path = tmp_path / "comment.md"

    result = runner.invoke(app, ["run", "main", "--pjnz-dir", str(pjnz_dir), "--comment-out", str(comment_path)])

    assert "98765" not in result.output
    assert "4321" not in result.output
    assert "98765" not in comment_path.read_text()


def test_run_catches_a_build_failure_for_one_file_without_aborting_the_batch(monkeypatch, tmp_path):
    def flaky_build_params(workspace, pjnz, output):  # noqa: ARG001
        if pjnz.stem == "broken":
            msg = "simulated build failure"
            raise params_module.ParamsBuildError(msg)

    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        return Verdict(indicator=name, passed=True, max_abs_diff=0.0, n_cells=10, n_failed=0)

    monkeypatch.setattr(params_module, "build_params", flaky_build_params)
    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "broken.PJNZ").touch()
    (pjnz_dir / "fine.PJNZ").touch()
    comment_path = tmp_path / "comment.md"

    result = runner.invoke(app, ["run", "main", "--pjnz-dir", str(pjnz_dir), "--comment-out", str(comment_path)])

    assert "ERROR" in result.output
    assert "PASS  fine.PJNZ" in result.output
    assert result.exit_code == 1


def test_run_accepts_the_working_tree_sentinel_without_crashing(monkeypatch, tmp_path):
    """Pin that `run_cmd` special-cases the WORKING_TREE sentinel.

    `git_utils.resolve_ref` has no SHA to resolve `WORKING_TREE` to --
    `run_cmd` must special-case it (same sentinel `_prepare_ref`/
    `prepare_source` already handle) rather than calling `resolve_ref`
    unconditionally.
    """

    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        return Verdict(indicator=name, passed=True, max_abs_diff=0.0, n_cells=10, n_failed=0)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "a.PJNZ").touch()
    comment_path = tmp_path / "comment.md"

    result = runner.invoke(
        app, ["run", git_utils.WORKING_TREE, "--pjnz-dir", str(pjnz_dir), "--comment-out", str(comment_path)]
    )

    assert result.exit_code == 0, result.output
    assert git_utils.WORKING_TREE in comment_path.read_text()


def test_run_skips_a_tag_scoped_indicator_for_an_untagged_pjnz(monkeypatch, tmp_path):
    """Confirm a tag-scoped indicator is skipped, not run, for an untagged PJNZ.

    A `tag(...)`-scoped Spectrum-track indicator (ticket 23's future
    territory) must be skipped, not spuriously run/failed, for a PJNZ
    whose tags don't satisfy it.
    """
    synthetic = dict(spectrum_track.SPECTRUM_INDICATORS)
    synthetic["custom_need"] = {
        "extract_leapfrog": lambda output_h5: None,  # noqa: ARG005
        "extract_spectrum": lambda pjnz: None,  # noqa: ARG005
        "tolerance": Tolerance(atol=1.0, rtol=0.01),
        "exclusions": (),
        "scope": tag("has_custom"),
    }
    monkeypatch.setattr(spectrum_track, "SPECTRUM_INDICATORS", synthetic)
    monkeypatch.setattr(spectrum_track.classify, "classify", lambda workspace, pjnz: frozenset())  # noqa: ARG005

    def fake_diff(name, output_h5, pjnz, pjnz_label=None):  # noqa: ARG001
        return Verdict(indicator=name, passed=True, max_abs_diff=0.0, n_cells=10, n_failed=0)

    monkeypatch.setattr(spectrum_track, "diff_spectrum_indicator", fake_diff)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "untagged.PJNZ").touch()
    comment_path = tmp_path / "comment.md"

    result = runner.invoke(app, ["run", "main", "--pjnz-dir", str(pjnz_dir), "--comment-out", str(comment_path)])

    assert result.exit_code == 0, result.output
    assert "SKIP  custom_need" in result.output
    assert "PASS  custom_need" not in result.output
