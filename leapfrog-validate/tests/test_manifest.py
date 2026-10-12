"""Tests for leapfrog_validate.manifest.

Per ticket 09's Answer: a manifest is reserved only for tags that aren't
derivable from a PJNZ's own contents at all (e.g. "custom-made for this
validation system" provenance) -- everything else comes from `classify`'s
zip peek / R import instead.
"""

import json
from pathlib import Path

import pytest

from leapfrog_validate import manifest


def _write(tmp_path: Path, content: str) -> Path:
    manifest_path = tmp_path / "manifest.json"
    manifest_path.write_text(content)
    return manifest_path


def test_load_manifest_returns_empty_dict_when_file_missing(tmp_path):
    assert manifest.load_manifest(tmp_path / "does-not-exist.json") == {}


def test_load_manifest_parses_filename_to_tags(tmp_path):
    manifest_path = _write(tmp_path, json.dumps({"foo.PJNZ": ["custom_made", "provenance:validation-system"]}))

    loaded = manifest.load_manifest(manifest_path)

    assert loaded == {"foo.PJNZ": frozenset({"custom_made", "provenance:validation-system"})}


def test_load_manifest_rejects_non_object_json(tmp_path):
    manifest_path = _write(tmp_path, json.dumps(["not", "an", "object"]))

    with pytest.raises(manifest.ManifestError):
        manifest.load_manifest(manifest_path)


def test_load_manifest_rejects_invalid_json(tmp_path):
    manifest_path = _write(tmp_path, "{not valid json,,,")

    with pytest.raises(manifest.ManifestError):
        manifest.load_manifest(manifest_path)


def test_load_manifest_rejects_a_bare_string_instead_of_a_tag_list(tmp_path):
    """Regression test: a bare string was silently exploded into one tag per character."""
    manifest_path = _write(tmp_path, json.dumps({"foo.PJNZ": "custom_made"}))

    with pytest.raises(manifest.ManifestError):
        manifest.load_manifest(manifest_path)


def test_load_manifest_rejects_non_string_items_in_tag_list(tmp_path):
    manifest_path = _write(tmp_path, json.dumps({"foo.PJNZ": ["ok", 123]}))

    with pytest.raises(manifest.ManifestError):
        manifest.load_manifest(manifest_path)


def test_manifest_tags_returns_empty_frozenset_for_unlisted_file(tmp_path):
    tags = manifest.manifest_tags({"other.PJNZ": frozenset({"x"})}, tmp_path / "foo.PJNZ")
    assert tags == frozenset()


def test_manifest_tags_looks_up_by_filename_not_full_path(tmp_path):
    loaded = {"foo.PJNZ": frozenset({"custom_made"})}

    tags = manifest.manifest_tags(loaded, tmp_path / "some" / "nested" / "dir" / "foo.PJNZ")

    assert tags == frozenset({"custom_made"})


def test_manifest_tags_distinguishes_same_basename_in_different_corpus_subfolders(tmp_path):
    """Ticket 16's flagged limitation, resolved here (ticket 20): corpus-relative keys.

    Two files named `foo.PJNZ` in different corpus subfolders (e.g. `US/`
    vs. `ETH/`) are indistinguishable by bare filename alone -- passing
    `corpus_root` lets the manifest key by the corpus-relative path instead,
    with no bare-filename entry at all to stay unambiguous between the two.
    """
    corpus_root = tmp_path / "corpus"
    (corpus_root / "US").mkdir(parents=True)
    (corpus_root / "ETH").mkdir(parents=True)
    us_file = corpus_root / "US" / "foo.PJNZ"
    eth_file = corpus_root / "ETH" / "foo.PJNZ"
    us_file.touch()
    eth_file.touch()
    loaded = {"US/foo.PJNZ": frozenset({"us_tag"}), "ETH/foo.PJNZ": frozenset({"eth_tag"})}

    assert manifest.manifest_tags(loaded, us_file, corpus_root=corpus_root) == frozenset({"us_tag"})
    assert manifest.manifest_tags(loaded, eth_file, corpus_root=corpus_root) == frozenset({"eth_tag"})


def test_manifest_tags_unions_bare_filename_and_corpus_relative_entries(tmp_path):
    """Both keys, if both present, apply -- neither silently shadows the other."""
    corpus_root = tmp_path / "corpus"
    (corpus_root / "ETH").mkdir(parents=True)
    nested = corpus_root / "ETH" / "foo.PJNZ"
    nested.touch()
    loaded = {"foo.PJNZ": frozenset({"bare_tag"}), "ETH/foo.PJNZ": frozenset({"relative_tag"})}

    tags = manifest.manifest_tags(loaded, nested, corpus_root=corpus_root)

    assert tags == frozenset({"bare_tag", "relative_tag"})


def test_manifest_tags_corpus_root_is_a_no_op_when_pjnz_is_outside_it(tmp_path):
    """A PJNZ outside `corpus_root` (e.g. a one-off file) falls back to bare-filename lookup only."""
    corpus_root = tmp_path / "corpus"
    corpus_root.mkdir()
    outside = tmp_path / "elsewhere" / "foo.PJNZ"
    outside.parent.mkdir()
    outside.touch()
    loaded = {"foo.PJNZ": frozenset({"bare_tag"})}

    assert manifest.manifest_tags(loaded, outside, corpus_root=corpus_root) == frozenset({"bare_tag"})
