"""Spectrum-comparison-track driver (ticket 22): CI entrypoint + local reproduce command.

A standalone script/CLI, deliberately **not** a subcommand grafted onto
`cli.py`'s `app` -- the task brief for this ticket explicitly scopes
`cli.py` as off-limits (ticket 21, the regression-track CI job, is being
built against it in parallel on another branch), mirroring
`select_fixtures.py`'s own precedent for the same reason (see that
module's docstring).

Builds its own PR comment by calling `leapfrog_validate` library functions
directly (`build`, `params`, `model_run`, `spectrum_extract`,
`spectrum_indicators`, `report_spectrum`) and controlling exactly what
reaches stdout -- never by shelling out to `leapfrog-validate` and
capturing its stdout, which (per `cli.py`'s own `diff`/`compare` commands)
prints a raw `max|delta|` number via `Verdict.summary()`. Every print
statement in this module is written to show only PASS/FAIL and filenames,
the same discipline ticket 22's brief requires of CI logs -- kept uniform
here even for `diff-one`'s local/interactive use, rather than relaxed for
"this isn't a public log", so there's exactly one printing convention to
audit rather than two.

Reuses `cli.py`'s already-implemented, already-reviewed corpus-sourcing
helpers (`_resolve_corpus_files`, `_pjnz_label`, `AZURE_CORPUS_PREFIX_DEFAULT`)
rather than re-implementing local-dir-vs-Azure-Blob resolution (including
its path-traversal safety checks) a second time -- ticket 20's own Comments
anticipated exactly this reuse ("Tickets 21/22 are where real
CI-against-a-live-container exercise would first happen"). Everything
these commands import from `cli.py` is read-only use of existing functions;
nothing in `cli.py` itself is modified by this ticket.

Two commands:

- `run REF (--pjnz-dir | --azure-container/--azure-account)` -- the CI
  entrypoint. Builds leapfrogr once at REF, runs every PJNZ in the
  resolved corpus against every blessed indicator's Spectrum comparator,
  and writes a rendered sticky-comment markdown file
  (`report_spectrum.format_comment`) for a workflow step to hand to
  `marocchino/sticky-pull-request-comment`.
- `diff-one REF PJNZ` -- the reproduce command baked into that comment: one
  PJNZ, every indicator, human-readable PASS/FAIL output.
"""

from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import Annotated

import typer

from leapfrog_validate import build, classify, git_utils, model_run, params
from leapfrog_validate.cli import AZURE_CORPUS_PREFIX_DEFAULT, _pjnz_label, _prepare_ref, _resolve_corpus_files
from leapfrog_validate.model_run import ModelRunError
from leapfrog_validate.params import ParamsBuildError
from leapfrog_validate.report_spectrum import IndicatorResult, PjnzResult, format_comment
from leapfrog_validate.spectrum_extract import SpectrumExtractError
from leapfrog_validate.spectrum_indicators import SPECTRUM_INDICATORS, diff_spectrum_indicator
from leapfrog_validate.subprocess_utils import summarize_error_reason

app = typer.Typer(help="Spectrum-comparison track: leapfrog output vs Spectrum's own PJNZ modvars.")

DEFAULT_CACHE_DIR = Path.home() / ".cache" / "leapfrog-validate"
REF_HELP = f"Git ref/SHA to build leapfrogr at, or '{git_utils.WORKING_TREE}' for your own uncommitted checkout."
CACHE_DIR_HELP = "Where per-ref build workspaces (git worktrees, installed R libraries) are cached."

# `_prepare_ref` itself is reused directly from `cli.py` (read-only import,
# not redefined here) -- it's a plain `git_utils.prepare_source` +
# `build.build_leapfrogr` wrapper with no CLI-specific logic, so
# duplicating its body here would just be a second copy to keep in sync.


def _needs_tags() -> bool:
    """Return whether any `SPECTRUM_INDICATORS` entry has a non-`ALL` `Scope`.

    Mirrors `cli.py`'s `compare_cmd`. The five blessed indicators are
    `ALL`-scoped and would need no `classify` call (which shells out to
    Rscript). Since ticket 23, `pmtct_need` (`has_pmtct`) and `cotrim_need`
    (`has_cotrim`) are tag-scoped, so this is `True` and every PJNZ is
    classified once.
    """
    return any(spec["scope"].tags is not None for spec in SPECTRUM_INDICATORS.values())


def _applicable_indicator_names(pjnz_tags: frozenset[str]) -> list[str]:
    """Return `SPECTRUM_INDICATORS` names whose `Scope` accepts `pjnz_tags`, sorted."""
    return sorted(name for name, spec in SPECTRUM_INDICATORS.items() if spec["scope"].applies_to(pjnz_tags))


def _classify_tags(workspace: build.BuildWorkspace, pjnz: Path) -> frozenset[str]:
    """`classify.classify(...)`, skipped entirely when no indicator needs tags (see `_needs_tags`).

    No `--manifest` support here (unlike `cli.py`'s `compare`/`classify`
    commands) -- out of this ticket's scope, since none of the five
    blessed Spectrum-track indicators need a non-derivable tag today;
    add it here the same way `compare_cmd` did if a future tag-scoped
    Spectrum indicator needs one.
    """
    if not _needs_tags():
        return frozenset()
    return classify.classify(workspace, pjnz)


@app.command("diff-one")
def diff_one_cmd(
    ref: Annotated[str, typer.Argument(help=REF_HELP)],
    pjnz: Annotated[Path, typer.Argument(exists=True, help="PJNZ file to compare against its own Spectrum modvars.")],
    configuration: Annotated[str, typer.Option(help="Model configuration to run.")] = "Spectrum",
    cache_dir: Annotated[
        Path, typer.Option("--cache-dir", envvar="LEAPFROG_VALIDATE_CACHE_DIR", help=CACHE_DIR_HELP)
    ] = DEFAULT_CACHE_DIR,
) -> None:
    """Build REF, run it against PJNZ, and diff every blessed indicator against Spectrum.

    This is the reproduce command baked into a failed PJNZ's entry in the
    Spectrum-comparison PR comment (see `report_spectrum._reproduce_command`).
    """
    repo_root = git_utils.find_repo_root()
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        workspace = _prepare_ref(repo_root, cache_dir, ref)

        try:
            pjnz_tags = _classify_tags(workspace, pjnz)
        except classify.ClassifyError as e:
            typer.echo(f"ERROR  could not classify: {e}", err=True)
            raise typer.Exit(1) from e

        applicable = _applicable_indicator_names(pjnz_tags)
        skipped = sorted(set(SPECTRUM_INDICATORS) - set(applicable))

        params_path = tmp_path / "params.h5"
        output_path = tmp_path / "output.h5"
        params.build_params(workspace, pjnz, params_path)
        model_run.run_model(workspace, params_path, output_path, configuration)

        all_passed = True
        for name in applicable:
            try:
                verdict = diff_spectrum_indicator(name, output_path, pjnz)
            except SpectrumExtractError as e:
                typer.echo(f"ERROR  {name}: could not extract Spectrum modvars: {e}", err=True)
                all_passed = False
                continue
            typer.echo(f"{'PASS' if verdict.passed else 'FAIL'}  {name}")
            all_passed = all_passed and verdict.passed
        for name in skipped:
            typer.echo(f"SKIP  {name}: pjnz tags {sorted(pjnz_tags)!r} don't satisfy its scope")

    raise typer.Exit(0 if all_passed else 1)


def _compare_one_pjnz_against_spectrum(
    workspace: build.BuildWorkspace,
    pjnz: Path,
    label: str,
    configuration: str,
    tmp_dir: Path,
) -> PjnzResult:
    """Build+run leapfrog for one PJNZ, then diff every blessed indicator against Spectrum.

    Mirrors `cli.py`'s `_process_one_pjnz_in_compare`: errors for one file
    (a build failure, or a PJNZ the avenir importer can't parse -- see
    `spectrum_extract.py`'s documented `BWA_ADULT_PJNZ`-style limitation)
    are caught and reported as that one file's result, never aborting the
    whole corpus walk.
    """
    typer.echo(f"\n== {label} ==")
    try:
        pjnz_tags = _classify_tags(workspace, pjnz)
    except classify.ClassifyError as e:
        typer.echo(f"  ERROR  could not classify: {e}", err=True)
        return PjnzResult(label=label, error=f"could not classify this PJNZ ({summarize_error_reason(e)})")

    applicable = _applicable_indicator_names(pjnz_tags)
    skipped = sorted(set(SPECTRUM_INDICATORS) - set(applicable))
    if pjnz_tags:
        typer.echo(f"  tags: {', '.join(sorted(pjnz_tags))}")

    safe_base = label.removesuffix(pjnz.suffix).replace("/", "__")
    params_path = tmp_dir / f"{safe_base}-params.h5"
    output_path = tmp_dir / f"{safe_base}-output.h5"
    try:
        params.build_params(workspace, pjnz, params_path)
        model_run.run_model(workspace, params_path, output_path, configuration)
    except (ParamsBuildError, ModelRunError) as e:
        typer.echo(f"  ERROR  could not build/run leapfrog for this PJNZ: {e}", err=True)
        reason = summarize_error_reason(e)
        return PjnzResult(label=label, error=f"could not build/run leapfrog for this PJNZ ({reason})")

    indicator_results = []
    for name in applicable:
        try:
            verdict = diff_spectrum_indicator(name, output_path, pjnz, pjnz_label=label)
        except SpectrumExtractError as e:
            typer.echo(f"  ERROR  {name}: could not extract Spectrum modvars: {e}", err=True)
            indicator_results.append(IndicatorResult(name=name, passed=False))
            continue
        typer.echo(f"  {'PASS' if verdict.passed else 'FAIL'}  {name}")
        indicator_results.append(IndicatorResult(name=name, passed=verdict.passed))
    for name in skipped:
        typer.echo(f"  SKIP  {name}: pjnz tags {sorted(pjnz_tags)!r} don't satisfy its scope")

    result = PjnzResult(label=label, indicators=tuple(indicator_results))
    typer.echo(f"  {label}: {'PASS' if result.passed else 'FAIL'}")
    return result


@app.command("run")
def run_cmd(  # noqa: PLR0913, PLR0917 - mirrors cli.py's compare_cmd's similarly flat corpus-source option surface
    ref: Annotated[str, typer.Argument(help=REF_HELP)],
    pjnz_dir: Annotated[
        Path | None,
        typer.Option(
            "--pjnz-dir",
            exists=True,
            file_okay=False,
            help="Local directory of PJNZ files. Mutually exclusive with --azure-container.",
        ),
    ] = None,
    azure_container: Annotated[
        str | None,
        typer.Option("--azure-container", help="Azure Blob container holding the private PJNZ corpus."),
    ] = None,
    azure_account: Annotated[
        str | None,
        typer.Option("--azure-account", help="Azure storage account name."),
    ] = None,
    azure_prefix: Annotated[
        str,
        typer.Option("--azure-prefix", help="Blob-name prefix the corpus lives under within the container."),
    ] = AZURE_CORPUS_PREFIX_DEFAULT,
    configuration: Annotated[str, typer.Option(help="Model configuration to run.")] = "Spectrum",
    comment_out: Annotated[
        Path,
        typer.Option("--comment-out", help="Where to write the rendered sticky-comment markdown."),
    ] = Path("spectrum-comment.md"),
    cache_dir: Annotated[
        Path, typer.Option("--cache-dir", envvar="LEAPFROG_VALIDATE_CACHE_DIR", help=CACHE_DIR_HELP)
    ] = DEFAULT_CACHE_DIR,
) -> None:
    """Run the Spectrum-comparison track against every PJNZ in the corpus.

    Writes a rendered sticky-comment markdown file for a CI step to hand to
    `marocchino/sticky-pull-request-comment`. Prints only PASS/FAIL lines
    and filenames to stdout/stderr -- never a raw diff magnitude or
    indicator value (ticket 22's CI-logs requirement) -- because this
    function builds the comment from in-memory `Verdict`s via
    `report_spectrum.format_comment`, not by scraping another command's
    stdout.

    Exits non-zero if any file fails any indicator -- this process's own
    exit code is informational, same as the GitHub Actions job it runs in:
    this track is never wired as a required status check, so a non-zero
    exit here does not block a merge.
    """
    repo_root = git_utils.find_repo_root()
    # `git_utils.resolve_ref` only supports a committed ref/SHA -- it has no
    # SHA to resolve `WORKING_TREE` to (ticket 19's Answer, same sentinel
    # `_prepare_ref`/`prepare_source` already special-case). There's no
    # frozen commit to bake into a reproduce command for an uncommitted
    # checkout either, so the sentinel itself is the most honest "ref_sha"
    # to display/reproduce with -- a developer re-running the baked
    # reproduce command against their own working tree gets the same
    # sentinel back, not a stale SHA from whenever this ran.
    ref_sha = ref if ref == git_utils.WORKING_TREE else git_utils.resolve_ref(repo_root, ref)

    # Same deliberate ordering as `cli.py`'s `compare_cmd` (ticket 20's
    # fixed real bug): consult the env-var fallback only when *neither*
    # CLI option was given at all, so an explicit --pjnz-dir is never
    # silently rejected just because AZURE_STORAGE_CONTAINER/_ACCOUNT
    # happen to be exported in the calling shell/CI job (as this track's
    # own workflow does, for its Azure-corpus default).
    if pjnz_dir is None and azure_container is None:
        azure_container = os.environ.get("AZURE_STORAGE_CONTAINER")
    if azure_container is not None:
        azure_account = azure_account or os.environ.get("AZURE_STORAGE_ACCOUNT")

    if (pjnz_dir is not None) == (azure_container is not None):
        typer.echo("Specify exactly one of --pjnz-dir or --azure-container as the PJNZ source.", err=True)
        raise typer.Exit(2)
    if azure_container is not None and not azure_account:
        typer.echo("--azure-account is required together with --azure-container.", err=True)
        raise typer.Exit(2)

    results: list[PjnzResult] = []
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        pjnz_files, corpus_root = _resolve_corpus_files(
            pjnz_dir, azure_container, azure_account, azure_prefix, tmp_path
        )
        workspace = _prepare_ref(repo_root, cache_dir, ref)

        for pjnz in pjnz_files:
            label = _pjnz_label(pjnz, corpus_root)
            results.append(_compare_one_pjnz_against_spectrum(workspace, pjnz, label, configuration, tmp_path))

    typer.echo("\n== Summary ==")
    for result in results:
        typer.echo(f"  {'PASS' if result.passed else 'FAIL'}  {result.label}")

    comment_out.write_text(format_comment(results, ref_given=ref, ref_sha=ref_sha))
    typer.echo(f"\nWrote comment to {comment_out}")

    raise typer.Exit(0 if all(r.passed for r in results) else 1)


def main() -> None:
    """Entry point: `python -m leapfrog_validate.spectrum_track <command> ...`."""
    app()


if __name__ == "__main__":
    main()
