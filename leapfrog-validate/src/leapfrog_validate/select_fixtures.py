"""Select PJNZ-derived fixtures for the cross-interface equality check.

Ticket 18 extends `.github/workflows/model-output-equality.yaml` (the
R/Python/C++ exact-equality check) to run across a small set of
shape-diverse configurations instead of one fixed fixture. The PJNZ file
backing each configuration must be picked via ticket 16's classifier tags
(`classify.shape_tags`/`classify.domain_tags`), not hand-listed by filename
-- this module is that selection logic, factored out from the GitHub
Actions YAML so it has a normal unit-test seam.

Three scenarios are supported:

- `baseline`: full age stratification, adult-only -- today's only fixture
  (`adult_parms_full.h5`, `HivFullAgeStratification`).
- `coarse`: same base PJNZ, `use_coarse_age_groups = TRUE` at
  `process_pjnz()` time (`adult_parms_coarse.h5`,
  `HivCoarseAgeStratification`) -- age stratification is a caller-supplied
  argument, not a PJNZ property (ticket 16's Comments), so this varies the
  invocation, not the file.
- `pmtct_child`: a PJNZ with real child/PMTCT data extracted, run under
  `Spectrum` (`spectrum_params.h5`) -- the smallest `ModelVariant` that
  actually exercises each language's child-model pars adapter
  (`HivFullAgeStratification`'s `get_pars` never reads `Hc` pars at all, so
  it can't catch a bug there).

`baseline`/`coarse` share one base PJNZ, picked via `pick_aim_only_pjnz`
(the cheap, zip-content-only `shape_tags` check). `pmtct_child` needs a
second, separately-justified PJNZ -- see `PMTCT_CHILD_PJNZ_NAME` below for
why a plain tag filter can't pick it the same deterministic way.

Every scenario's base PJNZ must also share the same underlying projection
options (`hts_per_year`, `t_ART_start`, `projection_start_year`,
`projection_period`): `cpp_interface/simulate_model.cpp` hardcodes these
rather than reading them from the params file, so a PJNZ with different
real options would make the C++ leg silently run with options that don't
match its own params -- a false-positive landmine, not a real adapter bug.
Verified empirically (not just assumed) that the `pmtct_child` PJNZ shares
these with the `baseline`/`coarse` PJNZ before relying on it -- see ticket
18's Comments.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

import typer

from leapfrog_validate import classify, git_utils
from leapfrog_validate.build import BuildWorkspace

REPO_ROOT = git_utils.find_repo_root(Path(__file__).parent)
PJNZ_DIR = REPO_ROOT / "leapfrogr" / "inst" / "pjnz"

# The `pmtct_child` scenario's PJNZ, by name. Ticket 16's Comments document
# that every real AIM-only fixture this repo has (this one included) tags
# `has_pmtct=True` identically -- the current tag vocabulary can confirm
# "carries nonzero PMTCT *input* data" but can't discriminate "and that
# input data actually produces a nonzero child epidemic once the child
# model runs" from "produces an all-zero one" (a finer domain property than
# ticket 16 built a tag for). Empirically, this repo's `bwa_aim-adult-art-*`
# fixture (the one `pick_aim_only_pjnz` lands on for baseline/coarse) is the
# latter -- every hc1_/hc2_ output array comes out exactly zero under
# `Spectrum`, which would make a deliberately-introduced child-model
# adapter bug undetectable via h5diff no matter which PJNZ backed it.
# `france_default.PJNZ` is the one real fixture confirmed (see ticket 18's
# Comments) to produce a genuinely nonzero child epidemic -- it's also
# already this repo's established fixture for exercising the `Spectrum`
# ModelVariant (`scripts/create_test_data.R`'s own comment: used for
# "testing Spectrum model variant" / non-AIDS excess mortality). Named here
# rather than re-derived by a tag filter alone; `require_has_pmtct` below
# still gates it against `domain_tags` so a future corpus change that
# silently broke this assumption would fail loudly rather than silently
# testing a meaningless all-zero case.
PMTCT_CHILD_PJNZ_NAME = "france_default.PJNZ"

# scenario name -> (params h5 filename under leapfrogr/tests/testthat/testdata/,
# model `configuration` to run it with). Both already exist as a side
# effect of `scripts/create_test_data.R` -- no new fixture needed.
_SCENARIO_ARTIFACTS: dict[str, tuple[str, str]] = {
    "baseline": ("adult_parms_full.h5", "HivFullAgeStratification"),
    "coarse": ("adult_parms_coarse.h5", "HivCoarseAgeStratification"),
    "pmtct_child": ("spectrum_params.h5", "Spectrum"),
}


class FixtureSelectionError(RuntimeError):
    """Raised when no candidate PJNZ satisfies a scenario's tag requirements."""


@dataclass(frozen=True)
class Scenario:
    """A fully resolved cross-interface equality scenario."""

    name: str
    pjnz: Path
    params_h5: str
    configuration: str


def pick_aim_only_pjnz(pjnz_dir: Path = PJNZ_DIR) -> Path:
    """Return the first (sorted, so deterministic) `aim_only`-tagged PJNZ in `pjnz_dir`.

    Uses `classify.shape_tags` (a cheap zip-content peek, no R needed) so
    the choice is tag-derived rather than a hardcoded filename -- if a new
    PJNZ were added to this directory, or an existing one reclassified, the
    selection follows automatically.
    """
    for pjnz in sorted(pjnz_dir.glob("*.PJNZ")):
        if "aim_only" in classify.shape_tags(pjnz):
            return pjnz
    msg = f"no aim_only PJNZ found under {pjnz_dir}"
    raise FixtureSelectionError(msg)


def pick_pmtct_child_pjnz(pjnz_dir: Path = PJNZ_DIR) -> Path:
    """Return the PJNZ backing the `pmtct_child` scenario.

    See `PMTCT_CHILD_PJNZ_NAME` for why this can't be a plain tag filter
    like `pick_aim_only_pjnz`. Still asserts the file exists and is
    `aim_only`-tagged (a cheap, always-available check) before returning it
    -- `require_has_pmtct` below adds the (more expensive) `has_pmtct` gate.
    """
    pjnz = pjnz_dir / PMTCT_CHILD_PJNZ_NAME
    if not pjnz.exists():
        msg = f"expected pmtct_child PJNZ not found: {pjnz}"
        raise FixtureSelectionError(msg)
    if "aim_only" not in classify.shape_tags(pjnz):
        msg = f"{pjnz} is not tagged aim_only; cannot use it for the pmtct_child scenario"
        raise FixtureSelectionError(msg)
    return pjnz


def require_has_pmtct(workspace: BuildWorkspace, pjnz: Path) -> None:
    """Raise `FixtureSelectionError` unless `pjnz` carries nonzero PMTCT data.

    A tripwire, not the primary selector (see `PMTCT_CHILD_PJNZ_NAME`): if
    the chosen PJNZ ever stopped carrying real PMTCT data, this scenario
    must fail loudly rather than silently testing a meaningless case.
    """
    tags = classify.domain_tags(workspace, pjnz)
    if "has_pmtct" not in tags:
        msg = (
            f"{pjnz} does not carry nonzero PMTCT data (domain_tags={sorted(tags)}); "
            "cannot use it for the pmtct_child scenario"
        )
        raise FixtureSelectionError(msg)


def select_scenario(
    name: str,
    workspace: BuildWorkspace | None,
    pjnz_dir: Path = PJNZ_DIR,
) -> Scenario:
    """Resolve `name` into a fully-specified `Scenario`.

    `workspace` is only needed for `pmtct_child` (it drives `domain_tags`,
    which shells out to Rscript against an installed `leapfrogr`); pass
    `None` for scenarios that only need the cheap `shape_tags` check.
    """
    if name not in _SCENARIO_ARTIFACTS:
        msg = f"unknown scenario '{name}'; expected one of {sorted(_SCENARIO_ARTIFACTS)}"
        raise FixtureSelectionError(msg)

    if name == "pmtct_child":
        if workspace is None:
            msg = "the pmtct_child scenario requires a BuildWorkspace to confirm has_pmtct via domain_tags"
            raise FixtureSelectionError(msg)
        pjnz = pick_pmtct_child_pjnz(pjnz_dir)
        require_has_pmtct(workspace, pjnz)
    else:
        pjnz = pick_aim_only_pjnz(pjnz_dir)

    params_h5, configuration = _SCENARIO_ARTIFACTS[name]
    return Scenario(name=name, pjnz=pjnz, params_h5=params_h5, configuration=configuration)


def main(argv: list[str] | None = None) -> int:
    """CLI entry point: print the resolved scenario as `KEY=value` lines.

    Designed to be piped straight into `$GITHUB_OUTPUT` from a workflow
    step (`... >> "$GITHUB_OUTPUT"`) -- stdout carries only `PARAMS_H5` and
    `CONFIGURATION`, matching GitHub Actions' step-output format; the
    chosen PJNZ's bare name (not sensitive, per the PRD) goes to stderr for
    log visibility only.

    Deliberately `argparse`, not `cli.py`'s `typer.Typer()` app: this is a
    standalone script invoked as `python -m leapfrog_validate.select_fixtures`
    from the GitHub Actions workflow, not a subcommand of the
    `leapfrog-validate` console script -- ticket 20 (in progress on a
    parallel branch) owns wiring tags into `cli.py`'s `compare`/`diff`
    scoping, and this ticket's scope is deliberately read-only with respect
    to that file. `typer` is still used for `.echo()` only, to match this
    package's existing print-vs-typer.echo convention (see `cli.py`).
    """
    parser = argparse.ArgumentParser(
        description="Select the PJNZ-derived fixture for a cross-interface equality scenario."
    )
    parser.add_argument("scenario", choices=sorted(_SCENARIO_ARTIFACTS))
    parser.add_argument(
        "--r-library",
        type=Path,
        default=None,
        help="R library leapfrogr was installed into (required for 'pmtct_child', to run domain_tags).",
    )
    args = parser.parse_args(argv)

    workspace = None
    if args.r_library is not None:
        workspace = BuildWorkspace(worktree=REPO_ROOT, r_library=args.r_library)

    scenario = select_scenario(args.scenario, workspace)
    typer.echo(f"PARAMS_H5={scenario.params_h5}")
    typer.echo(f"CONFIGURATION={scenario.configuration}")
    typer.echo(f"PJNZ={scenario.pjnz.name}", err=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
