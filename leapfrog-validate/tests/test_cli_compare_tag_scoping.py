"""CLI-level tests for `compare`'s tag-based indicator scoping (ticket 20).

A custom indicator can declare a `Scope` (via `indicators.tag(...)`)
restricting it to PJNZ carrying a specific tag, as computed by
`classify.classify`. `compare` must consult that scope per-PJNZ and skip
indicators that don't apply -- without any code/config change when a newly
added PJNZ happens to carry (or lack) the relevant tag.

Mirrors `test_cli_compare.py`'s mocking boundary (`_prepare_ref`,
`params.build_params`, `model_run.run_model`), and additionally mocks
`classify.classify` (its own real implementation needs a built R workspace
and a real zip -- covered elsewhere, not `compare`'s own job to re-prove).
"""

from pathlib import Path

import h5py
import numpy as np
import pytest
from typer.testing import CliRunner

from leapfrog_validate import classify, cli, indicators
from leapfrog_validate.build import BuildWorkspace
from leapfrog_validate.cli import app
from leapfrog_validate.diff import Tolerance

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
def fake_build_run_and_classify(monkeypatch):
    """Fake `_prepare_ref`/`build_params`/`run_model`/`classify.classify`.

    `scales` maps a PJNZ stem to a `(ref_scale, candidate_scale)` pair
    (equal -> PASS, differing -> FAIL for that file), same as
    `test_cli_compare.py`'s fixture. `pjnz_tags` maps a PJNZ stem to the
    tag set `classify.classify` should report for it -- standing in for the
    real zip-peek/R-import classifier, which isn't `compare`'s own job to
    re-prove here. `unclassifiable` is a set of PJNZ stems for which
    `classify.classify` should raise `ClassifyError`, simulating ticket 16's
    documented real limitation (some Goals-enabled PJNZ error out of
    `process_pjnz_ha`) without needing a real such fixture.
    """

    def fake_prepare_ref(repo_root: Path, cache_dir: Path, ref: str) -> BuildWorkspace:
        del repo_root, cache_dir
        worktree = Path(f"/fake/{ref}")
        return BuildWorkspace(worktree=worktree, r_library=worktree / "r-library")

    def fake_build_params(workspace: BuildWorkspace, pjnz: Path, output: Path) -> None:
        del workspace, pjnz, output

    def fake_run_model(workspace: BuildWorkspace, params_path: Path, output: Path, configuration: str) -> None:
        del configuration, workspace
        side = "candidate" if params_path.stem.endswith("-candidate-params") else "ref"
        pjnz_stem = params_path.stem.removesuffix(f"-{side}-params")
        ref_scale, candidate_scale = scales[pjnz_stem]
        scale = candidate_scale if side == "candidate" else ref_scale
        _write_output_h5(output, scale=scale)

    def fake_classify(workspace, pjnz, manifest_data=None, corpus_root=None):  # noqa: ARG001
        if pjnz.stem in unclassifiable:
            msg = f"{pjnz}: simulated classify failure"
            raise classify.ClassifyError(msg)
        return pjnz_tags.get(pjnz.stem, frozenset())

    scales: dict[str, tuple[float, float]] = {}
    unclassifiable: set[str] = set()
    pjnz_tags: dict[str, frozenset[str]] = {}
    monkeypatch.setattr(cli, "_prepare_ref", fake_prepare_ref)
    monkeypatch.setattr(cli.params, "build_params", fake_build_params)
    monkeypatch.setattr(cli.model_run, "run_model", fake_run_model)
    monkeypatch.setattr(cli.classify, "classify", fake_classify)
    return scales, pjnz_tags, unclassifiable


@pytest.fixture
def tag_scoped_indicator(monkeypatch):
    """Register one synthetic `has_pmtct`-scoped indicator alongside the five blessed ones."""
    synthetic = dict(indicators.INDICATORS)
    synthetic["pmtct_need"] = {
        "extract": indicators.extract_total_population,
        "tolerance": Tolerance(atol=1e-3, rtol=1e-6),
        "exclusions": (),
        "scope": indicators.tag("has_pmtct"),
    }
    monkeypatch.setattr(indicators, "INDICATORS", synthetic)
    return synthetic


def test_tag_scoped_indicator_runs_only_for_pjnz_carrying_the_tag(
    fake_build_run_and_classify, tag_scoped_indicator, tmp_path
):
    del tag_scoped_indicator  # fixture needed only for its INDICATORS-registry monkeypatching side effect
    scales, pjnz_tags, _unclassifiable = fake_build_run_and_classify
    scales["tagged"] = (1.0, 1.0)
    scales["untagged"] = (1.0, 1.0)
    pjnz_tags["tagged"] = frozenset({"has_pmtct"})
    pjnz_tags["untagged"] = frozenset()

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "tagged.PJNZ").touch()
    (pjnz_dir / "untagged.PJNZ").touch()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    assert result.exit_code == 0, result.output

    tagged_section, untagged_section = result.output.split("== untagged.PJNZ ==")
    assert "PASS  pmtct_need" in tagged_section
    assert "SKIP  pmtct_need" not in tagged_section
    assert "PASS  pmtct_need" not in untagged_section
    assert "SKIP  pmtct_need" in untagged_section


def test_newly_added_tagged_pjnz_is_automatically_eligible_with_no_config_change(
    fake_build_run_and_classify, tag_scoped_indicator, tmp_path
):
    """Adding a tagged PJNZ to the corpus dir is enough -- no registry/CLI change needed."""
    del tag_scoped_indicator  # fixture needed only for its INDICATORS-registry monkeypatching side effect
    scales, pjnz_tags, _unclassifiable = fake_build_run_and_classify
    scales["brand_new"] = (1.0, 1.5)
    pjnz_tags["brand_new"] = frozenset({"has_pmtct"})

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "brand_new.PJNZ").touch()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    # The synthetic indicator's tolerance is tight enough that the diverging
    # scale (1.0 vs 1.5) fails it -- proving it actually ran against the new
    # file, not just that the file was listed.
    assert "FAIL  pmtct_need" in result.output
    assert result.exit_code == 1


def test_untagged_pjnz_still_passes_on_the_five_blessed_indicators_alone(
    fake_build_run_and_classify, tag_scoped_indicator, tmp_path
):
    """Scoping out one indicator doesn't affect the always-applicable (`ALL`) ones."""
    del tag_scoped_indicator  # fixture needed only for its INDICATORS-registry monkeypatching side effect
    scales, pjnz_tags, _unclassifiable = fake_build_run_and_classify
    scales["untagged"] = (1.0, 1.0)
    pjnz_tags["untagged"] = frozenset()

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "untagged.PJNZ").touch()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    assert result.exit_code == 0, result.output
    assert "PASS  untagged.PJNZ" in result.output
    assert "SKIP  pmtct_need" in result.output


def test_classify_error_on_one_file_does_not_abort_the_whole_batch(
    fake_build_run_and_classify, tag_scoped_indicator, tmp_path
):
    """Ticket 16's documented `ClassifyError` limitation must degrade one file, not the run.

    Before this fix, an uncaught `ClassifyError` from `classify.classify`
    propagated straight out of `compare_cmd`, crashing with a raw traceback
    and losing results already computed for files processed before the bad
    one.
    """
    del tag_scoped_indicator  # fixture needed only for its INDICATORS-registry monkeypatching side effect
    scales, pjnz_tags, unclassifiable = fake_build_run_and_classify
    scales["good"] = (1.0, 1.0)
    pjnz_tags["good"] = frozenset({"has_pmtct"})
    unclassifiable.add("bad")

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "good.PJNZ").touch()
    (pjnz_dir / "bad.PJNZ").touch()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    assert not isinstance(result.exception, classify.ClassifyError), (
        f"ClassifyError leaked out uncaught instead of being reported cleanly: {result.exception!r}"
    )
    assert result.exit_code == 1
    assert "PASS  good.PJNZ" in result.output
    assert "ERROR" in result.output
    assert "bad.PJNZ" in result.output
    assert "== Summary ==" in result.output
    assert "FAIL  bad.PJNZ" in result.output


def test_manifest_flag_is_honored_even_with_no_tag_scoped_indicator_registered(
    fake_build_run_and_classify, tmp_path
):
    """`--manifest` must not be a silent no-op against today's all-`ALL` default registry.

    Before this fix, `needs_tags` was computed purely from whether any
    registered indicator has a non-`ALL` scope -- with today's five blessed
    (`ALL`-scoped) indicators, `classify.classify` (and so the manifest the
    user explicitly asked to load) was never consulted no matter what
    `--manifest` was given, with no message telling the user their flag had
    no effect.
    """
    scales, pjnz_tags, _unclassifiable = fake_build_run_and_classify
    scales["a"] = (1.0, 1.0)
    pjnz_tags["a"] = frozenset({"custom_made"})

    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "a.PJNZ").touch()
    manifest_path = tmp_path / "manifest.json"
    manifest_path.write_text("{}")

    result = runner.invoke(
        app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir), "--manifest", str(manifest_path)]
    )

    assert result.exit_code == 0, result.output
    assert "custom_made" in result.output
