"""Pure-function sticky-comment formatter for the Spectrum-comparison track.

Ticket 22's PR comment, fully separate from ticket 21's regression-track
comment (its own hidden marker, its own title, its own call to
`marocchino/sticky-pull-request-comment`) -- per the PRD's "Reporting & UI"
section, which requires the two comparisons never be merged into one.

This module is deliberately pure: `format_comment` takes already-computed
pass/fail verdicts (`PjnzResult`/`IndicatorResult`) and returns markdown
text, with no I/O, no model run, and critically no raw diff magnitude or
indicator value anywhere in its inputs -- `IndicatorResult` only carries a
name and a boolean. This is what makes the "CI logs/PR comment never print
actual diff values or indicator numbers" rule structurally enforced rather
than a discipline problem: there is no number to print by the time data
reaches this module, other than aggregate pass-rate counts/percentages
(explicitly allowed -- the PRD's own Phase 1 spec shows "overall % pass"
and a per-file pass-rate fraction in the comment).

Testable with synthetic data alone (`tests/test_report_spectrum.py`), per
the PRD's Testing Decisions -- no R build, no network, no real PJNZ.

**Errored files are excluded from the "% pass" denominator.** A real,
large-scale live run against this repo's private corpus (ticket 22's own
live verification, see this ticket's Comments) found most failures there
were PJNZ that could not be *built or run at all* (a pre-existing
`leapfrogr::process_pjnz_ha` limitation, same root cause ticket 16's
Comments already documented on one fixture, here hitting a much larger
share of the real corpus) -- not a real leapfrog-vs-Spectrum tolerance
disagreement. Counting those as ordinary "failed" cells made the headline
percentage ("3% pass") read as a catastrophic model regression when it was
actually "most of the corpus can't be run under current leapfrogr at all,"
a completely different, pre-existing problem this ticket didn't introduce
and isn't positioned to fix. Ticket 21's regression-track comment
independently hit and fixed the identical conflation on its own real run;
this module's three-way split (passed / failed / errored, with "% pass"
computed only over `passed + failed`) mirrors that fix so both tracks'
comments read the same way for the same underlying distinction.
"""

from __future__ import annotations

from dataclasses import dataclass, field

# Hidden-marker convention for `marocchino/sticky-pull-request-comment`'s
# `header:` input (ticket 06's research) -- distinct from whatever marker
# ticket 21's regression-track comment uses, so the two comments are found
# and updated independently rather than overwriting each other.
COMMENT_MARKER = "leapfrog-validate:spectrum-comparison"

TITLE = "leapfrog vs Spectrum comparison"


@dataclass(frozen=True)
class IndicatorResult:
    """One indicator's pass/fail verdict for one PJNZ -- no magnitude, by design."""

    name: str
    passed: bool


@dataclass(frozen=True)
class PjnzResult:
    """One PJNZ file's Spectrum-comparison outcome.

    `error`, if set, means the file could not be compared at all (e.g. the
    PJNZ importer rejected it, or leapfrog itself could not be built/run
    against it) -- reported in its own "could not be compared" category,
    distinct from a real tolerance failure (same spirit as `compare_cmd`'s
    `ERROR` line for a `ClassifyError`, and the reason this category is
    excluded from the "% pass" denominator -- see this module's docstring).
    `indicators` is empty whenever `error` is set.
    """

    label: str
    indicators: tuple[IndicatorResult, ...] = field(default_factory=tuple)
    error: str | None = None

    @property
    def errored(self) -> bool:
        """Whether this file could not be compared at all (build/run/extract failure)."""
        return self.error is not None

    @property
    def passed(self) -> bool:
        """Whole-file verdict: every indicator passed, and no extraction error."""
        if self.errored:
            return False
        return all(i.passed for i in self.indicators)

    @property
    def pass_rate_label(self) -> str:
        """`"<passed>/<total>"` indicator pass rate. Meaningless (never called) for an errored file."""
        total = len(self.indicators)
        passed = sum(1 for i in self.indicators if i.passed)
        return f"{passed}/{total}"


def _display_ref(ref_given: str, ref_sha: str) -> str:
    """Display as "branch-name (exact SHA)" if `ref_given` is a moving ref, else the bare SHA.

    Display-only convention from the PRD's Reporting & UI section -- never
    changes which SHA a reproduce command actually uses.
    """
    if ref_given == ref_sha:
        return f"`{ref_sha}`"
    return f"`{ref_given} ({ref_sha})`"


def _reproduce_command(pjnz_label: str, ref_sha: str) -> str:
    """Build a copy-pasteable local reproduce command for one failed PJNZ.

    Uses the exact resolved SHA, never a moving ref name (PRD's Reporting &
    UI section) -- the maintainer still needs their own local copy of the
    private PJNZ corpus (per-file content is never in this comment) at the
    given corpus-relative label.
    """
    return (
        f'leapfrog-validate build-params {ref_sha} "{pjnz_label}" -o params.h5\n'
        f"leapfrog-validate run {ref_sha} params.h5 -o output.h5 --configuration Spectrum\n"
        f'uv run python -m leapfrog_validate.spectrum_track diff-one {ref_sha} "{pjnz_label}"'
    )


def _format_entry(result: PjnzResult, ref_sha: str, *, errored: bool) -> str:
    if errored:
        lines = [f"- `{result.label}` -- could not be compared ({result.error})"]
    else:
        lines = [f"- `{result.label}` -- {result.pass_rate_label} indicators passed"]
    lines.append("  Reproduce:")
    lines.append("  ```")
    for cmd_line in _reproduce_command(result.label, ref_sha).splitlines():
        lines.append(f"  {cmd_line}")
    lines.append("  ```")
    return "\n".join(lines)


def format_comment(results: list[PjnzResult], ref_given: str, ref_sha: str) -> str:
    """Render the full Spectrum-track sticky-comment body as GitHub-flavored markdown.

    Matches ticket 21's comment shape (overall % pass, failed filenames
    with pass rate, reproduce commands) but is titled and marked
    differently, and always says it does not block merging. Written in
    short, plain sentences (one idea per sentence) by design, not just by
    accident -- this text is read by maintainers scanning a PR, not just by
    this module's own tests.

    "% pass" is computed only over files that could actually be compared
    (`passed + failed`, excluding `errored`) -- see this module's
    docstring for why a build/run failure must never silently count as a
    tolerance failure in that headline number.
    """
    errored = [r for r in results if r.errored]
    compared = [r for r in results if not r.errored]
    failed = [r for r in compared if not r.passed]
    n_compared = len(compared)
    n_passed = len(compared) - len(failed)

    lines = [
        f"<!-- {COMMENT_MARKER} -->",
        f"## {TITLE}",
        "",
        "This check compares leapfrog's output to Spectrum's own values.",
        "It reads Spectrum's values straight from each PJNZ file.",
        "It does not run Spectrum.exe.",
        "",
        "**This check never blocks a merge. It is for information only.**",
        "",
    ]

    if not results:
        lines.append("No PJNZ files were compared.")
        lines.append("")
        lines.append(f"Ref: {_display_ref(ref_given, ref_sha)}")
        return "\n".join(lines)

    if n_compared == 0:
        lines.append("No files could be compared. Every file hit a build or run error.")
    else:
        percent_pass = round(100 * n_passed / n_compared)
        lines.append(f"**Overall: {percent_pass}% pass ({n_passed}/{n_compared} files compared).**")
    if errored:
        lines.append(f"{len(errored)} file(s) could not be compared at all, and are not counted in that rate.")
    lines.append("")
    lines.append(f"Ref: {_display_ref(ref_given, ref_sha)}")
    lines.append("")

    if failed:
        lines.append("### Failed files")
        lines.append("")
        lines.append("Each failed file is listed below, with how many of its indicators passed.")
        lines.append("")
        for result in failed:
            lines.append(_format_entry(result, ref_sha, errored=False))
            lines.append("")
    elif n_compared > 0:
        lines.append("Every compared file passed.")
        lines.append("")

    if errored:
        lines.append("### Could not be compared")
        lines.append("")
        lines.append("Each file below hit a build or run error, not a tolerance failure.")
        lines.append("It is not counted as pass or fail.")
        lines.append("")
        for result in errored:
            lines.append(_format_entry(result, ref_sha, errored=True))
            lines.append("")

    return "\n".join(lines).rstrip() + "\n"
