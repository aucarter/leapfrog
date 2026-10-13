"""Integration tests driving the CLI/library primitives end-to-end.

Per the PRD's Testing Decisions, the primary seam is process/run/diff
invoked against small, non-sensitive PJNZ fixtures, asserting on the
resulting pass/fail verdict -- not on any one indicator extractor's
internals. This actually builds leapfrogr (R + the C++ core it wraps) at
a real git ref, so it needs R and network access to CRAN/r-universe; it's
skipped when Rscript isn't on PATH rather than faked with a mock.
"""

import shutil
import subprocess
from pathlib import Path

import pytest
from typer.testing import CliRunner

from leapfrog_validate import build, classify, git_utils, model_run, params
from leapfrog_validate.cli import app
from leapfrog_validate.diff import diff_indicator
from leapfrog_validate.indicators import INDICATORS

requires_r = pytest.mark.skipif(shutil.which("Rscript") is None, reason="R is not installed")

REPO_ROOT = git_utils.find_repo_root(Path(__file__).parent)
_PJNZ_DIR = REPO_ROOT / "leapfrogr" / "inst" / "pjnz"
FIXTURE_PJNZ = _PJNZ_DIR / "france_default.PJNZ"
SECOND_FIXTURE_PJNZ = _PJNZ_DIR / "bwa_aim-no-special-elig-numpmtct.PJNZ"
BWA_ADULT_FIXTURE = _PJNZ_DIR / "bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ"
BWA_NUMPMTCT_FIXTURE = SECOND_FIXTURE_PJNZ
GOALS_FIXTURE = REPO_ROOT / "goals" / "tests" / "resources" / "SouthAfrica.PJNZ"

runner = CliRunner()


@pytest.fixture(scope="module")
def head_sha():
    return git_utils.resolve_ref(REPO_ROOT, "HEAD")


@pytest.fixture(scope="module")
def cache_dir(tmp_path_factory):
    return tmp_path_factory.mktemp("leapfrog-validate-cache")


@pytest.fixture(scope="module")
def head_worktree(head_sha, cache_dir):
    worktree = git_utils.ensure_worktree(REPO_ROOT, head_sha, cache_dir)
    yield worktree
    subprocess.run(
        ["git", "-C", str(REPO_ROOT), "worktree", "remove", "--force", str(worktree)],
        check=False,
    )


@pytest.fixture(scope="module")
def head_workspace(head_worktree):
    return build.build_leapfrogr(head_worktree)


@requires_r
def test_build_params_produces_a_readable_h5(head_workspace, tmp_path):
    output = tmp_path / "params.h5"
    params.build_params(head_workspace, FIXTURE_PJNZ, output)
    assert output.exists()
    assert output.stat().st_size > 0


@requires_r
def test_build_params_relative_output_resolves_against_caller_cwd(head_workspace, tmp_path, monkeypatch):
    """Regression test.

    A relative -o path must land next to where the user ran the command,
    not inside the cached build worktree the Rscript subprocess happens to
    run from (see params.build_params).
    """
    monkeypatch.chdir(tmp_path)
    params.build_params(head_workspace, FIXTURE_PJNZ, Path("relative_params.h5"))

    assert (tmp_path / "relative_params.h5").exists()
    assert not (head_workspace.worktree / "relative_params.h5").exists()


@requires_r
def test_run_relative_output_resolves_against_caller_cwd(head_workspace, tmp_path, monkeypatch):
    """Same regression as above, for run's --output (see model_run.run_model)."""
    params_path = tmp_path / "params.h5"
    params.build_params(head_workspace, FIXTURE_PJNZ, params_path)

    monkeypatch.chdir(tmp_path)
    model_run.run_model(head_workspace, params_path, Path("relative_output.h5"), "Spectrum")

    assert (tmp_path / "relative_output.h5").exists()
    assert not (head_workspace.worktree / "relative_output.h5").exists()


@requires_r
def test_run_then_diff_of_a_ref_against_itself_passes(head_workspace, tmp_path):
    """The walking-skeleton acceptance case: same ref twice must PASS.

    This exercises build-params -> run -> diff exactly as a developer would
    invoke them, and pins down the "identical refs never falsely fail"
    correctness property the tolerance formula depends on.
    """
    params_path = tmp_path / "params.h5"
    params.build_params(head_workspace, FIXTURE_PJNZ, params_path)

    output_a = tmp_path / "output-a.h5"
    output_b = tmp_path / "output-b.h5"
    model_run.run_model(head_workspace, params_path, output_a, "Spectrum")
    model_run.run_model(head_workspace, params_path, output_b, "Spectrum")

    spec = INDICATORS["total_population"]
    arr_a = spec["extract"](output_a)
    arr_b = spec["extract"](output_b)
    verdict = diff_indicator(arr_a, arr_b, "total_population", spec["tolerance"])

    assert verdict.passed, verdict.summary()
    assert verdict.max_abs_diff == 0.0


@requires_r
def test_compare_same_ref_across_pjnz_directory_all_pass(head_workspace, head_sha, cache_dir, tmp_path):
    """Ticket 19's acceptance case: `compare` across a directory, same ref both sides, all PASS.

    Reuses `head_workspace`'s already-built cache (`cache_dir`) for both the
    `ref` and `candidate` side of `compare`, so this doesn't trigger a second
    build.
    """
    del head_workspace  # ensures the module-scoped build has already happened
    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    shutil.copy(FIXTURE_PJNZ, pjnz_dir / FIXTURE_PJNZ.name)
    shutil.copy(SECOND_FIXTURE_PJNZ, pjnz_dir / SECOND_FIXTURE_PJNZ.name)

    result = runner.invoke(
        app,
        ["--cache-dir", str(cache_dir), "compare", head_sha, head_sha, "--pjnz-dir", str(pjnz_dir)],
    )

    assert result.exit_code == 0, result.output
    assert f"PASS  {FIXTURE_PJNZ.name}" in result.output
    assert f"PASS  {SECOND_FIXTURE_PJNZ.name}" in result.output


@requires_r
def test_compare_working_tree_matches_head_and_second_run_skips_build(head_workspace, head_sha, cache_dir, tmp_path):
    """Ticket 19's other two acceptance cases, together since both need a real working-tree build.

    1. Working-tree support: `working-tree` compares equal to `HEAD` (no
       uncommitted changes exist under the hashed source dirs here), proving
       `<ref>` really does accept a pointer to the uncommitted checkout, not
       just committed refs.
    2. Build-skip caching: calling `compare` a second time with no relevant
       source change reuses the exact same materialized worktree and never
       re-touches its install marker -- i.e. the build/codegen/compile step
       is skipped.
    """
    del head_workspace
    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    shutil.copy(FIXTURE_PJNZ, pjnz_dir / FIXTURE_PJNZ.name)

    args = [
        "--cache-dir",
        str(cache_dir),
        "compare",
        git_utils.WORKING_TREE,
        head_sha,
        "--pjnz-dir",
        str(pjnz_dir),
    ]

    first = runner.invoke(app, args)
    assert first.exit_code == 0, first.output
    assert f"PASS  {FIXTURE_PJNZ.name}" in first.output

    worktree = git_utils.materialize_working_tree(REPO_ROOT, cache_dir)
    marker = worktree / ".leapfrog-validate" / "r-library" / ".install-complete"
    assert marker.exists()
    marker_mtime_after_first_run = marker.stat().st_mtime_ns

    second = runner.invoke(app, args)
    assert second.exit_code == 0, second.output
    assert marker.stat().st_mtime_ns == marker_mtime_after_first_run, "second compare run rebuilt instead of skipping"


@requires_r
@pytest.mark.parametrize("pjnz", [FIXTURE_PJNZ, BWA_ADULT_FIXTURE, BWA_NUMPMTCT_FIXTURE])
def test_domain_tags_for_aim_only_fixtures(head_workspace, pjnz):
    """Ground truth verified empirically: all three AIM-only fixtures carry non-zero PMTCT/cotrim inputs."""
    assert classify.domain_tags(head_workspace, pjnz) == frozenset({"has_pmtct", "has_cotrim"})


@requires_r
def test_domain_tags_for_goals_fixture(head_workspace):
    assert classify.domain_tags(head_workspace, GOALS_FIXTURE) == frozenset({"has_pmtct", "has_cotrim"})


@requires_r
def test_classify_combines_shape_and_domain_tags_for_a_real_fixture(head_workspace):
    assert classify.classify(head_workspace, FIXTURE_PJNZ) == frozenset({"aim_only", "has_pmtct", "has_cotrim"})
