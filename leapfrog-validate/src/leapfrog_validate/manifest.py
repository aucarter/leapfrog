"""Manifest of tags that can't be derived from a PJNZ's own contents.

Per ticket 09's Answer, reserved only for tags that aren't derivable at
all from what's inside the file -- e.g. "custom-made for this validation
system" provenance/purpose. Everything else comes from `classify`'s zip
peek / R import instead, so this stays deliberately small: a flat JSON
object mapping filename -> extra tags, not a general PJNZ database.

Keyed by bare filename (`pjnz.name`) by default -- the common case, and the
only option before corpus subfolders existed. The real uploaded corpus has
subfolders (e.g. `.../ETH/`), where two files can share a basename;
`manifest_tags`'s optional `corpus_root` resolves that (ticket 20) by also
checking a corpus-relative key (e.g. `"ETH/foo.PJNZ"`), unioned with
whatever the bare-filename key gives.
"""

import json
from pathlib import Path


class ManifestError(RuntimeError):
    """Raised when a manifest file isn't a JSON object of filename -> tags."""


def load_manifest(manifest_path: Path) -> dict[str, frozenset[str]]:
    """Load a JSON manifest: `{"<pjnz filename>": ["tag1", "tag2"]}`.

    A missing file is treated as an empty manifest -- most corpora won't
    need one at all.
    """
    try:
        text = manifest_path.read_text()
    except FileNotFoundError:
        return {}

    try:
        raw = json.loads(text)
    except json.JSONDecodeError as e:
        msg = f"{manifest_path}: invalid JSON ({e})"
        raise ManifestError(msg) from e

    if not isinstance(raw, dict):
        msg = f"{manifest_path}: expected a JSON object of filename -> tags, got {type(raw).__name__}"
        raise ManifestError(msg)

    for filename, tags in raw.items():
        if not isinstance(tags, list) or not all(isinstance(tag, str) for tag in tags):
            msg = f"{manifest_path}: '{filename}' must map to a list of tag strings, got {tags!r}"
            raise ManifestError(msg)

    return {filename: frozenset(tags) for filename, tags in raw.items()}


def manifest_tags(
    manifest: dict[str, frozenset[str]],
    pjnz: Path,
    corpus_root: Path | None = None,
) -> frozenset[str]:
    """Look up `pjnz`'s manifest tags.

    Always checks the bare-filename key (`pjnz.name`). If `corpus_root` is
    given and `pjnz` lives under it, also checks the corpus-relative key
    (POSIX-separated, e.g. `"ETH/foo.PJNZ"`) -- the two files-with-the-same-
    basename-in-different-subfolders case a bare filename alone can't
    distinguish (ticket 16's flagged limitation, resolved here). Tags from
    both keys are unioned if both are present, rather than one shadowing
    the other.

    Residual ambiguity: a bare key is still string-identical to the
    corpus-relative key of a file living at the corpus *root* (both are
    just `"foo.PJNZ"`), so it can't itself disambiguate a root-level file
    from a same-named one in a subfolder. Authors of colliding basenames
    should key both files by their (distinct) corpus-relative paths and
    skip the bare key for that name entirely.
    """
    tags = manifest.get(pjnz.name, frozenset())
    if corpus_root is not None:
        try:
            relative_key = pjnz.resolve().relative_to(corpus_root.resolve()).as_posix()
        except ValueError:
            relative_key = None
        if relative_key is not None:
            tags |= manifest.get(relative_key, frozenset())
    return tags
