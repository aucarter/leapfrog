"""CLI-level tests for `compare`, with build-params/run mocked out.

`compare`'s own job is iterating a PJNZ directory and aggregating per-file
pass/fail -- not building or running the model, which `test_cli_integration.py`
already covers end-to-end against real refs. Mocking `_prepare_ref`,
`params.build_params`, and `model_run.run_model` lets these run without R.
"""

from pathlib import Path

import h5py
import numpy as np
import pytest
from typer.testing import CliRunner

from leapfrog_validate import cli, git_utils
from leapfrog_validate.build import BuildWorkspace
from leapfrog_validate.cli import app

runner = CliRunner()


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


@pytest.fixture
def fake_build_and_run(monkeypatch):
    """Fake `_prepare_ref`/`build_params`/`run_model` driving a per-PJNZ, per-side scale.

    `scales` maps a PJNZ stem to a `(ref_scale, candidate_scale)` pair; equal
    scales produce a PASS, differing scales produce a FAIL for that file only.
    """

    def fake_prepare_ref(repo_root: Path, cache_dir: Path, ref: str) -> BuildWorkspace:
        del repo_root, cache_dir
        worktree = Path(f"/fake/{ref}")
        return BuildWorkspace(worktree=worktree, r_library=worktree / "r-library")

    def fake_build_params(workspace: BuildWorkspace, pjnz: Path, output: Path) -> None:
        del workspace, pjnz, output  # compare never inspects params.h5 content

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


def test_compare_flags_only_the_diverging_file(fake_build_and_run, tmp_path):
    fake_build_and_run["matching"] = (1.0, 1.0)
    fake_build_and_run["diverging"] = (1.0, 1.5)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "matching.PJNZ").touch()
    (pjnz_dir / "diverging.PJNZ").touch()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    assert result.exit_code == 1, result.output
    assert "PASS  matching.PJNZ" in result.output
    assert "FAIL  diverging.PJNZ" in result.output


def test_compare_all_matching_files_passes(fake_build_and_run, tmp_path):
    fake_build_and_run["a"] = (1.0, 1.0)
    fake_build_and_run["b"] = (1.0, 1.0)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "a.PJNZ").touch()
    (pjnz_dir / "b.PJNZ").touch()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    assert result.exit_code == 0, result.output
    assert "PASS  a.PJNZ" in result.output
    assert "PASS  b.PJNZ" in result.output


def test_compare_empty_pjnz_dir_errors(tmp_path):
    pjnz_dir = tmp_path / "empty"
    pjnz_dir.mkdir()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    assert result.exit_code == 2
    assert "No PJNZ files found" in result.output


def test_compare_accepts_working_tree_sentinel_as_either_side(fake_build_and_run, tmp_path, monkeypatch):
    """The `ref`/`candidate` positional args accept `git_utils.WORKING_TREE`, not just refs.

    `_prepare_ref` is mocked here (it's `git_utils.prepare_source` that does
    the actual sentinel dispatch, covered directly in test_git_utils.py) --
    this just pins that `compare` passes the literal string through unchanged
    rather than rejecting or special-casing it itself.
    """
    fake_build_and_run["a"] = (1.0, 1.0)

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "a.PJNZ").touch()

    seen_refs = []
    original = cli._prepare_ref

    def spy_prepare_ref(repo_root: Path, cache_dir: Path, ref: str) -> BuildWorkspace:
        seen_refs.append(ref)
        return original(repo_root, cache_dir, ref)

    monkeypatch.setattr(cli, "_prepare_ref", spy_prepare_ref)
    result = runner.invoke(app, ["compare", git_utils.WORKING_TREE, "main", "--pjnz-dir", str(pjnz_dir)])

    assert result.exit_code == 0, result.output
    assert seen_refs == [git_utils.WORKING_TREE, "main"]


def test_compare_one_pjnz_matches_exclusions_on_the_full_corpus_label(monkeypatch, tmp_path):
    """The exclusion identifier is the full label (`ETH/foo.PJNZ`), as in the Spectrum track.

    It must not be the disk-safe form (`ETH__foo`) used for tmp-file names.
    """
    seen: list[str | None] = []

    def fake_diff_one(name, a, b, pjnz):
        del a, b
        seen.append(pjnz)
        return cli.Verdict(indicator=name, passed=True, max_abs_diff=0.0, n_cells=1, n_failed=0)

    monkeypatch.setattr(cli.params, "build_params", lambda *_, **__: None)
    monkeypatch.setattr(cli.model_run, "run_model", lambda *_, **__: None)
    monkeypatch.setattr(cli, "_diff_one", fake_diff_one)

    workspace = BuildWorkspace(worktree=Path("/fake"), r_library=Path("/fake/r-library"))
    scoped = cli._ScopedPjnz(
        path=tmp_path / "ETH" / "foo.PJNZ", label="ETH/foo.PJNZ", indicator_names=["total_population"]
    )
    cli._compare_one_pjnz(cli._RefPair(ref=workspace, candidate=workspace), scoped, "Spectrum", tmp_path)

    assert seen == ["ETH/foo.PJNZ"]
