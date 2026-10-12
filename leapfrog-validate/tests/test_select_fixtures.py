"""Tests for leapfrog_validate.select_fixtures.

Ticket 18's cross-interface equality check needs to pick its PJNZ-derived
fixtures via ticket 16's classifier tags, not a hand-maintained filename
list. These tests exercise the selection logic against this repo's own
real, non-sensitive PJNZ fixtures (same pattern as `test_classify.py`),
with `domain_tags` monkeypatched where a real Rscript/leapfrogr build
would otherwise be required.
"""

import zipfile
from unittest.mock import Mock

import pytest

from leapfrog_validate import select_fixtures
from leapfrog_validate.build import BuildWorkspace
from leapfrog_validate.select_fixtures import PJNZ_DIR, REPO_ROOT, FixtureSelectionError, Scenario


def test_pick_aim_only_pjnz_returns_the_real_fixture_deterministically():
    pjnz = select_fixtures.pick_aim_only_pjnz()
    assert pjnz.parent == PJNZ_DIR
    # Deterministic (sorted) first match among this repo's real fixtures --
    # pinning the actual file so a silent change in sort order is caught.
    assert pjnz.name == "bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ"


def test_pick_aim_only_pjnz_raises_when_no_candidate_matches(tmp_path):
    goals_pjnz = tmp_path / "goals_only.PJNZ"
    with zipfile.ZipFile(goals_pjnz, "w") as z:
        z.writestr("goals_only.hv", "")

    with pytest.raises(FixtureSelectionError, match="no aim_only PJNZ"):
        select_fixtures.pick_aim_only_pjnz(tmp_path)


def test_pick_pmtct_child_pjnz_returns_the_named_real_fixture():
    pjnz = select_fixtures.pick_pmtct_child_pjnz()
    assert pjnz == PJNZ_DIR / "france_default.PJNZ"


def test_pick_pmtct_child_pjnz_raises_when_file_missing(tmp_path):
    with pytest.raises(FixtureSelectionError, match="not found"):
        select_fixtures.pick_pmtct_child_pjnz(tmp_path)


def test_pick_pmtct_child_pjnz_raises_when_not_aim_only(tmp_path, monkeypatch):
    monkeypatch.setattr(select_fixtures, "PMTCT_CHILD_PJNZ_NAME", "goals_only.PJNZ")
    goals_pjnz = tmp_path / "goals_only.PJNZ"
    with zipfile.ZipFile(goals_pjnz, "w") as z:
        z.writestr("goals_only.hv", "")

    with pytest.raises(FixtureSelectionError, match="not tagged aim_only"):
        select_fixtures.pick_pmtct_child_pjnz(tmp_path)


def test_select_scenario_baseline_needs_no_workspace():
    scenario = select_fixtures.select_scenario("baseline", workspace=None)

    assert scenario == Scenario(
        name="baseline",
        pjnz=PJNZ_DIR / "bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ",
        params_h5="adult_parms_full.h5",
        configuration="HivFullAgeStratification",
    )


def test_select_scenario_coarse_needs_no_workspace():
    scenario = select_fixtures.select_scenario("coarse", workspace=None)

    assert scenario.params_h5 == "adult_parms_coarse.h5"
    assert scenario.configuration == "HivCoarseAgeStratification"


def test_select_scenario_pmtct_child_requires_a_workspace():
    with pytest.raises(FixtureSelectionError, match="requires a BuildWorkspace"):
        select_fixtures.select_scenario("pmtct_child", workspace=None)


def test_select_scenario_pmtct_child_confirms_has_pmtct_via_domain_tags(monkeypatch):
    monkeypatch.setattr(select_fixtures.classify, "domain_tags", Mock(return_value=frozenset({"has_pmtct"})))
    workspace = BuildWorkspace(worktree=REPO_ROOT, r_library=REPO_ROOT)

    scenario = select_fixtures.select_scenario("pmtct_child", workspace=workspace)

    assert scenario.pjnz == PJNZ_DIR / "france_default.PJNZ"
    assert scenario.params_h5 == "spectrum_params.h5"
    assert scenario.configuration == "Spectrum"


def test_select_scenario_pmtct_child_raises_if_domain_tags_lack_has_pmtct(monkeypatch):
    # Tripwire for ticket 16's documented caveat: if this repo's chosen
    # fixture ever stopped carrying real PMTCT data, this scenario must
    # fail loudly rather than silently testing a meaningless all-zero case.
    monkeypatch.setattr(select_fixtures.classify, "domain_tags", Mock(return_value=frozenset()))
    workspace = BuildWorkspace(worktree=REPO_ROOT, r_library=REPO_ROOT)

    with pytest.raises(FixtureSelectionError, match="does not carry nonzero PMTCT data"):
        select_fixtures.select_scenario("pmtct_child", workspace=workspace)


def test_select_scenario_raises_for_unknown_scenario_name():
    with pytest.raises(FixtureSelectionError, match="unknown scenario"):
        select_fixtures.select_scenario("not-a-real-scenario", workspace=None)


def test_main_prints_github_actions_style_output(capsys, monkeypatch):
    monkeypatch.setattr(
        select_fixtures,
        "select_scenario",
        Mock(
            return_value=Scenario(
                name="coarse",
                pjnz=PJNZ_DIR / "bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ",
                params_h5="adult_parms_coarse.h5",
                configuration="HivCoarseAgeStratification",
            )
        ),
    )

    exit_code = select_fixtures.main(["coarse"])

    assert exit_code == 0
    out = capsys.readouterr().out
    assert "PARAMS_H5=adult_parms_coarse.h5" in out
    assert "CONFIGURATION=HivCoarseAgeStratification" in out
