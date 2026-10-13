"""Regression track: PR-ref-vs-main comparison -> a sticky PR comment (ticket 21).

Builds on ticket 20's `compare` plumbing in `leapfrog_validate.cli` as a
parallel module, not a refactor of `cli.py` itself -- `cli.py` is also
ticket 22's territory, built in parallel on a different branch, so this
avoids colliding with that build. One exception, kept deliberately small:
`cli._resolve_corpus_files` was split into a thin `typer`-facing wrapper
and a plain `_resolve_corpus_files_core` this module also calls, removing
a near-line-for-line duplicate of the Azure-download/empty-corpus logic a
review pass flagged -- external behavior of `cli.py`'s own function is
unchanged (see its docstring and the existing test suite, both unmodified).
Two halves:

- `compute_regression_report`: runs the same build/run/diff pipeline
  `compare` does, but returns structured per-PJNZ pass/fail counts instead
  of printing anything with an actual diff magnitude in it -- the thing CI
  logs for this job must never show (ticket 21's own checklist). Reuses
  `cli`'s private helpers (`_prepare_ref`, `_compare_one_pjnz`,
  `_pjnz_label`, `_applicable_indicator_names`, the Azure download trio)
  rather than re-deriving or duplicating their logic. Every call into
  `cli` goes through the `cli` module object itself (`cli._prepare_ref(...)`,
  not a direct `from ... import _prepare_ref`) so the existing
  `monkeypatch.setattr(cli, "_prepare_ref", ...)`-style test doubles
  already established in `test_cli_compare.py` also work here.
- `render_comment`: a pure function, verdicts in, markdown string out --
  the PRD's own prescribed test seam for this layer ("Testing Decisions":
  "asserting generated comment text against known input verdicts").

Also a standalone CLI (`python -m leapfrog_validate.report_regression`),
mirroring ticket 18's `select_fixtures.py` pattern: a small script a
GitHub Actions step shells out to, not folded into `cli.py`'s own `app`.
"""

from __future__ import annotations

import argparse
import sys
import tempfile
import traceback
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path

from leapfrog_validate import classify, cli, git_utils, manifest
from leapfrog_validate.comment_format import code_label, details, reproduce_block
from leapfrog_validate.subprocess_utils import summarize_error_reason

_PASS_MARK = "✅"
_FAIL_MARK = "❌"

# Identifies this track to `marocchino/sticky-pull-request-comment`'s
# `header:` input, so its find-or-update marker never collides with the
# Spectrum-comparison track's separate sticky comment (ticket 22, built in
# parallel on a different branch/workflow file). Not embedded in the
# rendered markdown itself -- the Action manages its own hidden marker at
# a layer above this module (ticket 06's research). The workflow reads this
# value via `--print-header` (see `main`) rather than hardcoding a second,
# independently-typed copy of the same string in the YAML -- code review
# flagged that drift risk during ticket 21's build.
STICKY_COMMENT_HEADER = "leapfrog-vs-leapfrog-regression"

# How many leading hex characters of a full commit SHA to show in
# human-facing display text. The reproduce command always uses the full
# SHA (see `_reproduce_command_lines`) -- this only shortens what a human
# reads, per the PRD's "branch-name (exact SHA)" display convention.
SHORT_SHA_LEN = 12


class RegressionTrackError(RuntimeError):
    """Raised for this track's own usage/validation errors (not a comparison failure)."""


@dataclass(frozen=True)
class ResolvedRef:
    """A git ref exactly as given by the caller, plus the commit SHA it resolved to.

    `given` is whatever was passed in (a branch name, a SHA, anything
    `git rev-parse` accepts); `sha` is always the full 40-character commit
    SHA. `display()` follows the PRD's display convention: a branch/moving
    ref shows as `"branch-name (exact SHA)"`; a ref that was already a bare
    SHA shows as just the (shortened) SHA, since showing it twice would be
    redundant.
    """

    given: str
    sha: str

    def display(self) -> str:
        """Human-facing display text, per the PRD's branch-name-plus-SHA convention."""
        short = self.sha[:SHORT_SHA_LEN]
        if self.given == self.sha:
            return short
        return f"{self.given} ({short})"


@dataclass(frozen=True)
class IndicatorOutcome:
    """One indicator's pass/fail verdict for one PJNZ -- no magnitude, by design."""

    name: str
    passed: bool


@dataclass(frozen=True)
class PjnzRegressionResult:
    """One PJNZ file's regression-track result, already rolled up to indicator counts.

    Deliberately carries no magnitude (no `max_abs_diff`, no cell counts)
    -- `indicators` only carries each applicable indicator's name and
    whether it passed. That's exactly the finest grain this track's
    reporting is allowed to surface; the actual diff values live only in
    the `Verdict` objects this is built from, which
    `compute_regression_report` never logs or returns. `n_passed`/
    `n_applicable` are kept as their own fields (redundant with
    `indicators` where it's populated) rather than derived, so an errored
    file -- which has no `indicators` to derive from -- still reports
    `0/0` without a special case.

    `errored` distinguishes "could not be compared at all" (a classify or
    build/run failure, e.g. a pre-existing leapfrogr data-compatibility
    issue on one specific PJNZ -- see `compute_regression_report`'s
    `classify.ClassifyError`/`params.ParamsBuildError`/
    `model_run.ModelRunError` handling) from "was compared and some
    indicator didn't match" (a genuine regression) -- `error_reason` carries
    a short, public-comment-safe summary of what actually went wrong (see
    `subprocess_utils.summarize_error_reason`), so a maintainer can tell
    what to reproduce locally rather than just "something failed".

    **An `errored` file still counts as a failure.** An earlier version of
    this field excluded errored files from the pass/fail verdict and
    headline pass rate entirely, reasoning that a pre-existing,
    PR-unrelated `process_pjnz_ha` limitation (hit by ~97 of 143 real
    corpus files during this ticket's own live verification) shouldn't be
    conflated with a genuine regression. That reasoning was overridden on
    request: if the check cannot build or run a file at all, that is
    itself a real problem worth seeing as a failure on every PR, not a
    number quietly carved out of the headline -- `errored` still changes
    *how* the failure is described (a reason, not an indicator count), not
    whether it counts.
    """

    label: str
    n_passed: int
    n_applicable: int
    errored: bool = False
    error_reason: str | None = None
    indicators: tuple[IndicatorOutcome, ...] = field(default_factory=tuple)

    @property
    def passed(self) -> bool:
        """Whether this file was compared and every applicable indicator matched.

        Always `False` for an `errored` file -- it was never actually
        compared, so "passed" would overclaim. Use `errored` to tell that
        case apart from a genuine indicator mismatch.
        """
        if self.errored:
            return False
        return self.n_passed == self.n_applicable

    @property
    def pass_rate(self) -> float:
        """Fraction of this file's applicable indicators that passed. 1.0 if none were applicable."""
        return self.n_passed / self.n_applicable if self.n_applicable else 1.0


@dataclass(frozen=True)
class RegressionReport:
    """Everything `render_comment` needs for one regression-track run."""

    ref: ResolvedRef
    candidate: ResolvedRef
    results: tuple[PjnzRegressionResult, ...]
    # Threaded through into the reproduce command (see `_reproduce_command_lines`)
    # so it stays correct even if a caller ever passes a non-default
    # configuration -- never assumed to match `run_cmd`'s own CLI default,
    # which could drift independently of this track's actual choice.
    configuration: str = "Spectrum"

    @property
    def errored_results(self) -> tuple[PjnzRegressionResult, ...]:
        """Every PJNZ that could not be compared at all (a classify or build/run failure).

        Still counted as failed everywhere below -- this property exists
        for rendering (showing *why* a failure happened), not for
        excluding anything from the pass/fail verdict.
        """
        return tuple(r for r in self.results if r.errored)

    @property
    def overall_passed(self) -> bool:
        """Whether every PJNZ passed -- an `errored` file counts as not passed."""
        return all(r.passed for r in self.results)

    @property
    def overall_pass_rate(self) -> float:
        """The headline number: fraction of all files that passed overall.

        Deliberately coarser than `PjnzRegressionResult.pass_rate` (which
        is per-file, indicator-level) -- mirrors the PRD's dashboard split
        between a corpus-wide "headline pass rate" and each row's own
        "indicators-passed fraction". Computed over every result,
        `errored` included -- a build/run failure counts as a failure here
        (see `PjnzRegressionResult`'s docstring for why).
        """
        if not self.results:
            return 1.0
        return sum(1 for r in self.results if r.passed) / len(self.results)

    @property
    def failed_results(self) -> tuple[PjnzRegressionResult, ...]:
        """Every PJNZ that failed -- a real indicator mismatch, or a build/run/classify error."""
        return tuple(r for r in self.results if not r.passed)

    @property
    def passed_results(self) -> tuple[PjnzRegressionResult, ...]:
        """Every PJNZ that passed."""
        return tuple(r for r in self.results if r.passed)


def _reproduce_command_lines(ref_sha: str, candidate_sha: str, label: str, configuration: str) -> list[str]:
    """Build the copy-pasteable local-reproduce command for one failed PJNZ.

    Uses the full, already-resolved commit SHAs for both sides -- never a
    moving ref name like `main` -- per the PRD's reproduce-command
    requirement. `<label>` doubles as the expected local relative path to
    the file: a developer who downloads it from the private corpus
    (`leapfrog-validate/infra/README.md`) preserving its corpus-relative
    path gets a command that works unmodified. `configuration` is spelled
    out explicitly on the `run` steps rather than relying on it happening
    to match `run_cmd`'s own CLI default -- this track's actual choice
    could drift from that default independently.
    """
    run_candidate = (
        f"leapfrog-validate run {candidate_sha} candidate-params.h5 "
        f"-o candidate-output.h5 --configuration {configuration} && \\"
    )
    return [
        f'# First, get "{label}" from the private corpus.',
        "# See leapfrog-validate/infra/README.md for how.",
        f'leapfrog-validate build-params {ref_sha} "{label}" -o ref-params.h5 && \\',
        f'leapfrog-validate build-params {candidate_sha} "{label}" -o candidate-params.h5 && \\',
        f"leapfrog-validate run {ref_sha} ref-params.h5 -o ref-output.h5 --configuration {configuration} && \\",
        run_candidate,
        f'leapfrog-validate diff ref-output.h5 candidate-output.h5 --pjnz "{label}"',
    ]


def _format_number(value: float) -> str:
    """Render a tolerance constant compactly, with no literal decimal point.

    Whole numbers render as plain integers; fractional values render as
    bare scientific notation (`1e-3`, not `1.0e-03`). A decimal point here
    would look exactly like a diff magnitude to a reader skimming the
    comment even though this is a static, public tolerance config value,
    not a computed result -- avoiding it keeps that distinction visible
    at a glance, not just true in principle.
    """
    if value == int(value):
        return str(int(value))
    mantissa, _, exponent = f"{value:.0e}".partition("e")
    return f"{mantissa}e{int(exponent)}"


def _indicators_checked_section() -> list[str]:
    """Render the static "indicators checked" collapsible section.

    Lists every indicator this track's registry defines, with its own
    atol (the fixed amount a cell is allowed to differ by) -- rtol is
    uniform across all of them (ticket 07's Answer) and stated once in
    the prose above this section instead of repeated per row. This is
    registry metadata, not a per-run result -- unaffected by which files
    actually got compared.
    """
    registry = cli.indicators.INDICATORS
    lines = [
        "<details>",
        f"<summary>Indicators checked ({len(registry)})</summary>",
        "",
        "| Indicator | Fixed amount (atol) |",
        "|---|---|",
    ]
    for name in sorted(registry):
        atol = registry[name]["tolerance"].atol
        lines.append(f"| `{name}` | {_format_number(atol)} |")
    lines.append("")
    lines.append("</details>")
    return lines


def render_comment(report: RegressionReport) -> str:
    """Render `report` as the regression track's sticky-comment markdown.

    Follows the PRD's Phase 1 "Reporting & UI" spec: one top line with
    overall % pass (and the raw counts behind it, e.g. "56/56"), then only
    the failed PJNZ, each in its own collapsed block with its own pass rate and a
    copy-pasteable reproduce command with resolved SHAs, and no plot links (no hosted
    drill-down view exists yet). Never says "Track A"/"Track B" -- named
    by what it compares instead. Prose is written in short, plain,
    one-idea sentences (headers/labels stay terse noun phrases); code,
    filenames, SHAs, and commands are left exactly as they are.

    A build/run/classify failure (`errored`) counts as a failed file here,
    same as a genuine indicator mismatch -- see `PjnzRegressionResult`'s
    docstring for why. It still gets its own distinct wording (the actual
    error reason, not an indicator pass count) so the two stay visually
    distinguishable even though both now count the same way.

    Layout, top to bottom: headline, ref, the tolerance this track checks
    against, a collapsed "Succeeded files" section (so a scan of the
    comment sees the good news first), then a collapsed "Failed files"
    section. Each failed file is its own collapsed block inside it, which
    opens to the per-indicator detail and a collapsed reproduce command. ✅/❌ marks are a readability aid only -- the
    underlying pass/fail boolean they stand for is unchanged.
    """
    lines = [
        "## leapfrog vs leapfrog regression",
        "",
        f"This check compares `{report.ref.display()}` against `{report.candidate.display()}`.",
        "It runs the full PJNZ corpus.",
        "",
    ]

    status_mark = _PASS_MARK if report.overall_passed else _FAIL_MARK
    status = "PASS" if report.overall_passed else "FAIL"
    pct = round(report.overall_pass_rate * 100)
    n_total = len(report.results)
    n_passed = len(report.passed_results)
    lines.append(f"**Result: {status_mark} {status} -- {pct}% of files pass ({n_passed}/{n_total}).**")
    lines.append("")
    rtol_display = _format_number(cli.indicators.RTOL)
    lines.append(
        f"A value counts as matching if it is within {rtol_display} times the reference value, "
        "plus a small fixed amount. The fixed amount is different for each indicator."
    )
    lines.append("")
    lines.extend(_indicators_checked_section())
    lines.append("")

    passed = report.passed_results
    if passed:
        lines.extend(
            details(
                f"{_PASS_MARK} Succeeded files ({len(passed)}/{n_total})",
                [f"- `{result.label}`" for result in passed],
            )
        )
        lines.append("")

    failed = report.failed_results
    genuine_failed = [r for r in failed if not r.errored]
    errored = report.errored_results
    if not failed:
        lines.append("All files passed.")
        lines.append("")
    else:
        lines.extend(
            details(
                f"{_FAIL_MARK} Failed files ({len(failed)}/{n_total})",
                _failed_file_blocks(report, genuine_failed, errored),
            )
        )
        lines.append("")

    lines.append("---")
    lines.append("This check never shows real numbers. It only shows pass or fail, and file names.")

    return "\n".join(lines)


def _failed_file_blocks(
    report: RegressionReport,
    genuine_failed: list[PjnzRegressionResult],
    errored: tuple[PjnzRegressionResult, ...],
) -> list[str]:
    """Build one closed `<details>` block per failed file, for the "Failed files" body.

    Each block opens to the file's indicator ticks/crosses (or the reason
    it could not be compared) and its own closed "Reproduce this" block.
    """
    lines = ["These files did not pass. Open a file to see why.", ""]
    for result in genuine_failed:
        body = [f"- {_PASS_MARK if outcome.passed else _FAIL_MARK} {outcome.name}" for outcome in result.indicators]
        body.append("")
        body.extend(_reproduce_details_block(report, result.label))
        summary = f"{code_label(result.label)} -- {result.n_passed}/{result.n_applicable} indicators passed"
        lines.extend(details(summary, body))
        lines.append("")
    for result in errored:
        body = [f"Could not be compared: {result.error_reason or 'unknown error'}", ""]
        body.extend(_reproduce_details_block(report, result.label))
        lines.extend(details(f"{code_label(result.label)} -- could not be compared", body))
        lines.append("")
    return lines[:-1]


def _reproduce_details_block(report: RegressionReport, label: str) -> list[str]:
    """Build the closed "Reproduce this" block for one PJNZ."""
    return reproduce_block(_reproduce_command_lines(report.ref.sha, report.candidate.sha, label, report.configuration))


@dataclass(frozen=True)
class _CorpusSource:
    """Resolved PJNZ files plus the root they're relative to."""

    files: list[Path]
    root: Path


def _resolve_corpus(
    *,
    pjnz_dir: Path | None,
    azure_container: str | None,
    azure_account: str | None,
    azure_prefix: str,
    tmp_path: Path,
) -> _CorpusSource:
    """Resolve the PJNZ source (local dir or Azure Blob) to its files + corpus root.

    Delegates to `cli._resolve_corpus_files_core` -- the plain,
    `typer`-independent core `cli._resolve_corpus_files` itself wraps for
    `compare`'s own CLI error handling -- rather than re-deriving the same
    Azure-download/empty-check logic a second time (split out of this
    module during ticket 21's build after code review flagged the
    near-duplicate). Translates `cli.CorpusError` (an unsafe blob path, or
    an empty corpus) into this module's own `RegressionTrackError`, so
    callers of this module only ever need to catch one exception type.
    """
    try:
        pjnz_files, corpus_root = cli._resolve_corpus_files_core(
            pjnz_dir, azure_container, azure_account, azure_prefix, tmp_path
        )
    except cli.CorpusError as e:
        msg = str(e)
        raise RegressionTrackError(msg) from e
    return _CorpusSource(files=pjnz_files, root=corpus_root)


def compute_regression_report(  # noqa: PLR0913
    *,
    repo_root: Path,
    cache_dir: Path,
    ref: str,
    candidate: str,
    pjnz_dir: Path | None = None,
    azure_container: str | None = None,
    azure_account: str | None = None,
    azure_prefix: str = cli.AZURE_CORPUS_PREFIX_DEFAULT,
    manifest_path: Path | None = None,
    configuration: str = "Spectrum",
    log: Callable[[str], None] = print,
) -> RegressionReport:
    """Run the regression track (`ref` vs. `candidate`, every PJNZ in the corpus) and roll it up.

    `ref`/`candidate` are resolved up front via `git_utils.resolve_ref`, so
    every reported SHA is exact even if the caller passed a moving ref
    (e.g. `candidate="origin/main"`) -- ticket 21's CI job always passes
    the PR's own head SHA as `ref` and a moving branch ref as `candidate`
    (see the scope note in ticket 21's briefing: "candidate is literally
    main, ref is the PR's head SHA").

    `log` receives only progress lines, PASS/FAIL, and filenames -- never a
    diff magnitude or indicator value, matching ticket 21's CI-log
    requirement. Defaults to `print`; the workflow script below passes it
    straight through to stdout, so CI's own job log is exactly as
    restricted as a human reading it live.

    Exactly one of `pjnz_dir`/`azure_container` must be given, same
    mutual-exclusivity rule as `compare`'s own CLI (not re-validated by a
    shared function, since `compare_cmd`'s version raises `typer.Exit`,
    meaningless outside a Click dispatch).
    """
    if (pjnz_dir is not None) == (azure_container is not None):
        msg = "Specify exactly one of pjnz_dir or azure_container as the PJNZ source."
        raise RegressionTrackError(msg)
    if azure_container is not None and not azure_account:
        msg = "azure_account is required together with azure_container."
        raise RegressionTrackError(msg)

    ref_sha = git_utils.resolve_ref(repo_root, ref)
    candidate_sha = git_utils.resolve_ref(repo_root, candidate)

    manifest_data = manifest.load_manifest(manifest_path) if manifest_path is not None else {}
    needs_tags = manifest_path is not None or any(
        spec["scope"].tags is not None for spec in cli.indicators.INDICATORS.values()
    )

    results: list[PjnzRegressionResult] = []
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        corpus = _resolve_corpus(
            pjnz_dir=pjnz_dir,
            azure_container=azure_container,
            azure_account=azure_account,
            azure_prefix=azure_prefix,
            tmp_path=tmp_path,
        )

        log(f"Building ref workspace ({ref_sha[:SHORT_SHA_LEN]}) ...")
        ref_workspace = cli._prepare_ref(repo_root, cache_dir, ref_sha)
        log(f"Building candidate workspace ({candidate_sha[:SHORT_SHA_LEN]}) ...")
        candidate_workspace = cli._prepare_ref(repo_root, cache_dir, candidate_sha)
        workspaces = cli._RefPair(ref=ref_workspace, candidate=candidate_workspace)

        for pjnz in corpus.files:
            label = cli._pjnz_label(pjnz, corpus.root)
            log(f"Comparing {label} ...")
            try:
                pjnz_tags = (
                    classify.classify(ref_workspace, pjnz, manifest_data, corpus_root=corpus.root)
                    if needs_tags
                    else frozenset()
                )
            except classify.ClassifyError as e:
                log(f"ERROR {label}: could not classify")
                reason = f"could not classify this PJNZ ({summarize_error_reason(e)})"
                results.append(
                    PjnzRegressionResult(label=label, n_passed=0, n_applicable=0, errored=True, error_reason=reason)
                )
                continue

            applicable = cli._applicable_indicator_names(pjnz_tags)
            scoped = cli._ScopedPjnz(path=pjnz, label=label, indicator_names=applicable)
            try:
                verdicts = cli._compare_one_pjnz(workspaces, scoped, configuration, tmp_path)
            except (cli.params.ParamsBuildError, cli.model_run.ModelRunError) as e:
                # A real corpus file can hit a pre-existing leapfrogr
                # limitation unrelated to this ticket (e.g. ticket 16's
                # documented `process_pjnz_ha` issue, confirmed live
                # against the real corpus during this ticket's build --
                # see Comments) -- same shape as ticket 20's own fix for
                # `classify.ClassifyError` not aborting the whole batch.
                # Logs only the exception's type to the CI job log, never
                # its full message (which embeds the subprocess's captured
                # stdout/stderr) -- `summarize_error_reason` extracts a
                # short, public-comment-safe line from it instead, so the
                # PR comment can still say *what* broke, not just *that*
                # something did.
                log(f"ERROR {label}: could not build/run ({type(e).__name__})")
                reason = f"could not build/run leapfrog for this PJNZ ({summarize_error_reason(e)})"
                results.append(
                    PjnzRegressionResult(label=label, n_passed=0, n_applicable=0, errored=True, error_reason=reason)
                )
                continue
            n_passed = sum(1 for v in verdicts if v.passed)
            indicator_outcomes = tuple(IndicatorOutcome(name=v.indicator, passed=v.passed) for v in verdicts)
            results.append(
                PjnzRegressionResult(
                    label=label, n_passed=n_passed, n_applicable=len(verdicts), indicators=indicator_outcomes
                )
            )
            log(f"{'PASS' if n_passed == len(verdicts) else 'FAIL'} {label}")

    return RegressionReport(
        ref=ResolvedRef(given=ref, sha=ref_sha),
        candidate=ResolvedRef(given=candidate, sha=candidate_sha),
        results=tuple(results),
        configuration=configuration,
    )


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Compute the leapfrog-vs-leapfrog regression track and render a PR-comment markdown file."
    )
    parser.add_argument(
        "--print-header",
        action="store_true",
        help=(
            "Print the STICKY_COMMENT_HEADER constant and exit immediately -- lets the "
            "workflow read the sticky-comment `header:` value from this module's own "
            "constant instead of a second, independently-typed copy in the YAML."
        ),
    )
    # Not `required=True`: `--print-header` is a standalone early-exit mode (see `main`)
    # that needs none of the options below.
    parser.add_argument("--ref", help="Git ref/SHA to treat as the PR's own side (usually the PR head SHA).")
    parser.add_argument(
        "--candidate", default="origin/main", help="Git ref/SHA to compare against (default: origin/main)."
    )
    parser.add_argument(
        "--repo-root", type=Path, default=None, help="Monorepo root. Defaults to the enclosing git repo."
    )
    parser.add_argument(
        "--cache-dir", type=Path, default=cli.DEFAULT_CACHE_DIR, help="Where per-ref build workspaces are cached."
    )
    parser.add_argument(
        "--pjnz-dir",
        type=Path,
        default=None,
        help="Local directory of PJNZ files. Mutually exclusive with --azure-container.",
    )
    parser.add_argument("--azure-container", default=None, help="Azure Blob container holding the private PJNZ corpus.")
    parser.add_argument("--azure-account", default=None, help="Azure storage account name.")
    parser.add_argument(
        "--azure-prefix", default=cli.AZURE_CORPUS_PREFIX_DEFAULT, help="Blob-name prefix the corpus lives under."
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=None,
        dest="manifest_path",
        help="JSON manifest of tags that can't be derived from file contents.",
    )
    parser.add_argument(
        "--configuration",
        default="Spectrum",
        help="Model configuration to run (ticket 21: the regression track always uses Spectrum).",
    )
    parser.add_argument("--out", type=Path, help="Where to write the rendered comment markdown.")
    return parser


def main(argv: list[str] | None = None) -> int:
    """CLI entry point: compute the report, write the comment, return the process exit code.

    Writes a comment file even on failure -- including an infrastructure
    failure that happens before any PJNZ is compared -- so the GitHub
    Actions step that posts it always has something to post, matching the
    "found-or-updated" sticky-comment pattern never silently skipping an
    update. Never writes a traceback into the comment file itself (that
    goes to stderr, i.e. the CI job log, which is allowed to show
    non-diff error detail); the comment stays short and numberless.
    """
    args = _build_arg_parser().parse_args(argv)

    if args.print_header:
        sys.stdout.write(f"{STICKY_COMMENT_HEADER}\n")
        return 0

    if not args.ref or not args.out:
        sys.stderr.write("--ref and --out are required unless --print-header is given.\n")
        return 2

    repo_root = args.repo_root or git_utils.find_repo_root()

    try:
        report = compute_regression_report(
            repo_root=repo_root,
            cache_dir=args.cache_dir,
            ref=args.ref,
            candidate=args.candidate,
            pjnz_dir=args.pjnz_dir,
            azure_container=args.azure_container,
            azure_account=args.azure_account,
            azure_prefix=args.azure_prefix,
            manifest_path=args.manifest_path,
            configuration=args.configuration,
        )
    except (RegressionTrackError, git_utils.GitError) as e:
        sys.stderr.write(f"leapfrog-validate regression track could not run: {e}\n")
        args.out.write_text(
            "## leapfrog vs leapfrog regression\n\nThis check could not run. See the workflow logs for details.\n"
        )
        return 1
    except Exception:
        traceback.print_exc()
        args.out.write_text(
            "## leapfrog vs leapfrog regression\n\n"
            "This check hit an unexpected error. See the workflow logs for details.\n"
        )
        return 1

    args.out.write_text(render_comment(report))
    return 0 if report.overall_passed else 1


if __name__ == "__main__":
    sys.exit(main())
