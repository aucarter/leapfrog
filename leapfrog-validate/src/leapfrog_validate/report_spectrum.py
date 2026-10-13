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
reaches this module, other than aggregate pass-rate counts/percentages and
the registry's own static tolerance constants (both explicitly allowed --
the PRD's own Phase 1 spec shows "overall % pass" and a per-file pass-rate
fraction in the comment, and a tolerance config value is not a diff result
at all, just a public, already-in-source-control number).

Testable with synthetic data alone (`tests/test_report_spectrum.py`), per
the PRD's Testing Decisions -- no R build, no network, no real PJNZ.

**A build/run/classify failure counts as a failed file.** An earlier
version of this module excluded these ("errored") files from the "% pass"
denominator entirely, on the reasoning that a pre-existing, PR-unrelated
`leapfrogr::process_pjnz_ha` limitation (ticket 16's Comments) was being
conflated with genuine leapfrog-vs-Spectrum tolerance disagreements. That
reasoning was overridden on request: if the check cannot build or run a
file, that is itself a real problem worth seeing as a failure on every
PR, not a number quietly carved out of the headline. A build/run/classify
failure is still labelled distinctly from a tolerance mismatch (its own
"could not be compared" reason is shown, not an indicator pass count), but
it now counts in `n_failed`/the headline pass rate like any other failure.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from leapfrog_validate.comment_format import code_label, details, reproduce_block
from leapfrog_validate.indicators import Scope
from leapfrog_validate.spectrum_indicators import SPECTRUM_INDICATORS, SPECTRUM_RTOL

# Hidden-marker convention for `marocchino/sticky-pull-request-comment`'s
# `header:` input (ticket 06's research) -- distinct from whatever marker
# ticket 21's regression-track comment uses, so the two comments are found
# and updated independently rather than overwriting each other.
COMMENT_MARKER = "leapfrog-validate:spectrum-comparison"

TITLE = "leapfrog vs Spectrum comparison"

_PASS_MARK = "✅"
_FAIL_MARK = "❌"


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
    against it). It still counts as a failure (see this module's
    docstring) -- `error` only changes how the failure is described, not
    whether it counts. `indicators` is empty whenever `error` is set.
    """

    label: str
    indicators: tuple[IndicatorResult, ...] = field(default_factory=tuple)
    error: str | None = None

    @property
    def errored(self) -> bool:
        """Whether this file could not be compared at all (build/run/extract/classify failure)."""
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


def _format_number(value: float) -> str:
    """Render a tolerance constant compactly, with no literal decimal point.

    Whole numbers render as plain integers (`50`, `1`); fractional values
    render as bare scientific notation (`1e-3`, not `1.0e-03`). A decimal
    point here would look exactly like a diff magnitude to a reader
    skimming the comment (and to the test guarding against one) even
    though this is a static, public tolerance config value, not a
    computed result -- avoiding it keeps that distinction visible at a
    glance, not just true in principle.
    """
    if value == int(value):
        return str(int(value))
    mantissa, _, exponent = f"{value:.0e}".partition("e")
    return f"{mantissa}e{int(exponent)}"


def _describe_scope(scope: Scope) -> str:
    """Say which files an indicator runs on. A tag-scoped indicator is skipped, silently, for other files."""
    if scope.tags is None:
        return "All files"
    tags = " and ".join(f"`{t}`" for t in sorted(scope.tags))
    return f"Files tagged {tags}"


def _indicators_checked_section() -> list[str]:
    """Render the static "indicators checked" collapsible section.

    Lists every indicator this track's registry defines, with its own
    atol (the fixed amount a cell is allowed to differ by) -- rtol is
    uniform across all of them (PRD's "Starting numbers" table) and stated
    once in the prose above this section instead of repeated per row. This
    is registry metadata, not a per-run result -- unaffected by which
    files actually got compared.
    """
    lines = [
        "<details>",
        f"<summary>Indicators checked ({len(SPECTRUM_INDICATORS)})</summary>",
        "",
        "| Indicator | Fixed amount (atol) | Applies to |",
        "|---|---|---|",
    ]
    for name in sorted(SPECTRUM_INDICATORS):
        spec = SPECTRUM_INDICATORS[name]
        lines.append(f"| `{name}` | {_format_number(spec['tolerance'].atol)} | {_describe_scope(spec['scope'])} |")
    lines.append("")
    lines.append("</details>")
    return lines


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


def _format_entry(result: PjnzResult, ref_sha: str) -> list[str]:
    """Build one closed `<details>` block for a failed file: its indicator ticks/crosses, then "Reproduce this"."""
    if result.errored:
        summary = f"{code_label(result.label)} -- could not be compared"
        body = [f"Could not be compared: {result.error}"]
    else:
        summary = f"{code_label(result.label)} -- {result.pass_rate_label} indicators passed"
        body = [f"- {_PASS_MARK if i.passed else _FAIL_MARK} {i.name}" for i in result.indicators]
    body.append("")
    body.extend(reproduce_block(_reproduce_command(result.label, ref_sha).splitlines()))
    return details(summary, body)


def format_comment(results: list[PjnzResult], ref_given: str, ref_sha: str) -> str:
    """Render the full Spectrum-track sticky-comment body as GitHub-flavored markdown.

    Matches ticket 21's comment shape (overall % pass, failed filenames
    with pass rate, reproduce commands) but is titled and marked
    differently, and always says it does not block merging. Written in
    short, plain sentences (one idea per sentence) by design, not just by
    accident -- this text is read by maintainers scanning a PR, not just by
    this module's own tests.

    The headline counts every file -- a build/run/classify failure counts
    as a failed file, same as a genuine tolerance mismatch (see this
    module's docstring for why). Each failed entry still says *why* it
    failed, so a build/run failure and a tolerance failure stay
    distinguishable even though both now count the same way.

    Layout, top to bottom: headline, ref, the tolerance this track checks
    against, a collapsed "Succeeded files" section (so a scan of the
    comment sees the good news first), then a collapsed "Failed files"
    section. Each failed file is its own collapsed block inside it, which
    opens to the per-indicator detail and a collapsed reproduce command. ✅/❌ marks are a readability aid only -- the
    underlying pass/fail boolean they stand for is unchanged.
    """
    failed = [r for r in results if not r.passed]
    passed = [r for r in results if r.passed]
    n_total = len(results)
    n_passed = len(passed)

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

    status_mark = _PASS_MARK if not failed else _FAIL_MARK
    status = "PASS" if not failed else "FAIL"
    percent_pass = round(100 * n_passed / n_total)
    lines.append(f"**Result: {status_mark} {status} -- {percent_pass}% of files pass ({n_passed}/{n_total}).**")
    lines.append("")
    lines.append(f"Ref: {_display_ref(ref_given, ref_sha)}")
    lines.append("")
    lines.append(
        f"A value counts as matching if it is within {_format_number(SPECTRUM_RTOL * 100)}% "
        "of the other value, plus a small fixed amount. The fixed amount is different for each indicator."
    )
    lines.append("")
    lines.extend(_indicators_checked_section())
    lines.append("")

    if passed:
        lines.extend(
            details(
                f"{_PASS_MARK} Succeeded files ({n_passed}/{n_total})",
                [f"- `{result.label}`" for result in passed],
            )
        )
        lines.append("")

    if failed:
        failed_body = ["These files did not pass. Open a file to see why.", ""]
        for result in failed:
            failed_body.extend(_format_entry(result, ref_sha))
            failed_body.append("")
        lines.extend(details(f"{_FAIL_MARK} Failed files ({len(failed)}/{n_total})", failed_body[:-1]))
        lines.append("")
    else:
        lines.append("Every file passed.")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"
