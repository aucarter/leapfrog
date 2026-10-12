"""Classify PJNZ files by configuration shape and domain properties.

Two derivation tiers, per ticket 09's Answer (tracing `leapfrog-compare`'s
`pjnz_classify.py` zip-content-sniff technique):

- `shape_tags`: a cheap zip-content peek, no PJNZ import needed. Presence
  of a `.HV` member distinguishes Goals-enabled PJNZ from AIM-only, mapping
  onto the `Goals` vs `Spectrum`/AIM-only `ModelVariant`s (see CONTEXT.md).
- `domain_tags`: a data-level sniff. Imports the PJNZ via the same
  `leapfrog::process_pjnz()` path `leapfrog_validate.params.build_params`
  already uses, then checks whether the PMTCT/cotrimoxazole input arrays
  are all-zero -- a PJNZ that doesn't use PMTCT/cotrim still carries the
  corresponding input variable, just zeroed out, so file presence alone
  can't tell the two cases apart.
"""

import zipfile
from pathlib import Path

from leapfrog_validate.build import BuildWorkspace
from leapfrog_validate.manifest import manifest_tags
from leapfrog_validate.subprocess_utils import run_checked

_CLASSIFY_SCRIPT = Path(__file__).parent / "r_scripts" / "classify_pjnz.R"


class ClassifyError(RuntimeError):
    """Raised when classifying a PJNZ file via Rscript fails."""


def shape_tags(pjnz: Path) -> frozenset[str]:
    """Derive configuration-shape tags from `pjnz`'s zip member names.

    A `.HV` member (case-insensitive) marks a Goals-enabled PJNZ; its
    absence marks AIM-only -- confirmed against this repo's own fixtures
    (`goals/tests/resources/SouthAfrica.PJNZ` carries `.HV`, the
    `leapfrogr/inst/pjnz/*.PJNZ` fixtures don't).

    Raises `ClassifyError` on a corrupt/non-zip/missing file, matching
    `domain_tags`'s always-fail-as-a-domain-error convention.
    """
    try:
        with zipfile.ZipFile(pjnz) as z:
            has_hv = any(name.lower().endswith(".hv") for name in z.namelist())
    except (OSError, zipfile.BadZipFile) as e:
        msg = f"{pjnz}: not a readable PJNZ/zip file ({e})"
        raise ClassifyError(msg) from e
    return frozenset({"goals"}) if has_hv else frozenset({"aim_only"})


def _parse_domain_tags_output(stdout: str) -> frozenset[str]:
    """Parse `classify_pjnz.R`'s `key=TRUE`/`key=FALSE` lines into a tag set."""
    tags = set()
    for line in stdout.splitlines():
        key, _, value = line.strip().partition("=")
        if value == "TRUE":
            tags.add(key)
    return frozenset(tags)


def domain_tags(workspace: BuildWorkspace, pjnz: Path) -> frozenset[str]:
    """Derive domain tags (`has_pmtct`, `has_cotrim`) by importing `pjnz`.

    Shells out to `classify_pjnz.R`, which runs `leapfrog::process_pjnz()`
    and checks whether the PMTCT/cotrimoxazole input arrays are all-zero.
    Raises `ClassifyError` (rather than guessing) if the import itself
    fails -- notably, this currently happens for some Goals-enabled PJNZ
    due to a pre-existing `process_pjnz_ha` limitation unrelated to this
    classifier (see ticket 16's comments).
    """
    pjnz = pjnz.resolve()
    result = run_checked(
        ["Rscript", str(_CLASSIFY_SCRIPT), str(workspace.r_library), str(pjnz)],
        cwd=workspace.worktree,
        error_cls=ClassifyError,
        error_context="classify-pjnz",
    )
    return _parse_domain_tags_output(result.stdout)


def classify(
    workspace: BuildWorkspace,
    pjnz: Path,
    manifest_data: dict[str, frozenset[str]] | None = None,
    corpus_root: Path | None = None,
) -> frozenset[str]:
    """Return the full tag set for `pjnz`: shape tags | domain tags | manifest tags.

    `manifest_data` (from `manifest.load_manifest`) covers tags that can't
    be derived from `pjnz` at all -- provenance/purpose, per ticket 09.
    `corpus_root`, if given, is forwarded to `manifest.manifest_tags` so a
    manifest can disambiguate two same-named PJNZ in different corpus
    subfolders (ticket 20) -- see its docstring.
    """
    tags = shape_tags(pjnz) | domain_tags(workspace, pjnz)
    if manifest_data:
        tags |= manifest_tags(manifest_data, pjnz, corpus_root=corpus_root)
    return tags
