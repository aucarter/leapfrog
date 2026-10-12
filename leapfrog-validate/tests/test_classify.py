"""Tests for leapfrog_validate.classify.

Shape tags come from a cheap zip-content peek (no R needed) -- verified
against this repo's own real, non-sensitive PJNZ fixtures rather than
synthetic zips, since the whole point is distinguishing real Goals-enabled
PJNZ (carries a `.HV` member, per `goals/tests/resources/SouthAfrica.PJNZ`)
from real AIM-only PJNZ (per ticket 09's Answer, tracing
`pjnz_classify.py`'s existing technique).
"""

import zipfile
from pathlib import Path
from unittest.mock import Mock

import pytest

from leapfrog_validate import classify, git_utils

REPO_ROOT = git_utils.find_repo_root(Path(__file__).parent)
GOALS_FIXTURE = REPO_ROOT / "goals" / "tests" / "resources" / "SouthAfrica.PJNZ"
AIM_ONLY_FIXTURE = REPO_ROOT / "leapfrogr" / "inst" / "pjnz" / "france_default.PJNZ"


def test_shape_tags_detects_goals_from_hv_member():
    assert classify.shape_tags(GOALS_FIXTURE) == frozenset({"goals"})


def test_shape_tags_detects_aim_only_when_no_hv_member():
    assert classify.shape_tags(AIM_ONLY_FIXTURE) == frozenset({"aim_only"})


def test_shape_tags_is_case_insensitive_on_extension(tmp_path):
    pjnz = tmp_path / "lowercase.pjnz"
    with zipfile.ZipFile(pjnz, "w") as z:
        z.writestr("lowercase.dp", "")
        z.writestr("lowercase.hv", "")

    assert classify.shape_tags(pjnz) == frozenset({"goals"})


def test_shape_tags_raises_classify_error_for_a_corrupt_file(tmp_path):
    not_a_zip = tmp_path / "broken.PJNZ"
    not_a_zip.write_bytes(b"not actually a zip file")

    with pytest.raises(classify.ClassifyError):
        classify.shape_tags(not_a_zip)


def test_parse_domain_tags_output_includes_only_true_tags():
    stdout = "has_pmtct=TRUE\nhas_cotrim=FALSE\n"
    assert classify._parse_domain_tags_output(stdout) == frozenset({"has_pmtct"})


def test_parse_domain_tags_output_empty_when_all_false():
    stdout = "has_pmtct=FALSE\nhas_cotrim=FALSE\n"
    assert classify._parse_domain_tags_output(stdout) == frozenset()


def test_parse_domain_tags_output_both_true():
    stdout = "has_pmtct=TRUE\nhas_cotrim=TRUE\n"
    assert classify._parse_domain_tags_output(stdout) == frozenset({"has_pmtct", "has_cotrim"})


def test_classify_unions_shape_domain_and_manifest_tags(monkeypatch):
    monkeypatch.setattr(classify, "domain_tags", Mock(return_value=frozenset({"has_pmtct"})))
    manifest_data = {"SouthAfrica.PJNZ": frozenset({"custom_made"})}

    tags = classify.classify(Mock(), GOALS_FIXTURE, manifest_data)

    assert tags == frozenset({"goals", "has_pmtct", "custom_made"})


def test_classify_without_manifest_data_omits_manifest_tags(monkeypatch):
    monkeypatch.setattr(classify, "domain_tags", Mock(return_value=frozenset()))

    tags = classify.classify(Mock(), AIM_ONLY_FIXTURE)

    assert tags == frozenset({"aim_only"})


def test_classify_passes_corpus_root_through_to_manifest_tags(monkeypatch, tmp_path):
    """Ticket 20: `corpus_root` lets manifest tags disambiguate same-named corpus files."""
    monkeypatch.setattr(classify, "domain_tags", Mock(return_value=frozenset()))
    corpus_root = tmp_path / "corpus"
    (corpus_root / "ETH").mkdir(parents=True)
    nested = corpus_root / "ETH" / AIM_ONLY_FIXTURE.name
    nested.write_bytes(AIM_ONLY_FIXTURE.read_bytes())
    manifest_data = {f"ETH/{AIM_ONLY_FIXTURE.name}": frozenset({"eth_specific"})}

    tags = classify.classify(Mock(), nested, manifest_data, corpus_root=corpus_root)

    assert tags == frozenset({"aim_only", "eth_specific"})
