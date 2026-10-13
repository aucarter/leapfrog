"""Shared subprocess-run-and-check helper.

Every module here shells out to git/uv/R and needs the same failure shape
(exit code, full stdout/stderr, command, cwd) surfaced as a domain-specific
exception.
"""

import subprocess
from pathlib import Path

# Lines these exception messages carry that are never the actual error text
# -- R's own trailer ("Calls:"/"Execution halted"), and this module's own
# command/cwd/section-marker scaffolding (see `run_checked`). Stripped by
# `summarize_error_reason` so the line it returns is the real error, not
# boilerplate around it.
_BOILERPLATE_LINE_PREFIXES = (
    "Calls:",
    "Execution halted",
    "In addition:",
    "Warning message",
    "--- stdout ---",
    "--- stderr ---",
    "command:",
    "cwd:",
)


def run_checked(
    cmd: list[str],
    cwd: Path,
    error_cls: type[Exception],
    env: dict | None = None,
    error_context: str = "",
) -> subprocess.CompletedProcess:
    """Run `cmd`, raising `error_cls` with full command/cwd/stdout/stderr on a nonzero exit."""
    result = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        msg = (
            f"{error_context} failed (exit {result.returncode})\n"
            f"command: {' '.join(cmd)}\n"
            f"cwd: {cwd}\n"
            f"--- stdout ---\n{result.stdout}\n"
            f"--- stderr ---\n{result.stderr}"
        )
        raise error_cls(msg)
    return result


def summarize_error_reason(exc: Exception, max_len: int = 300) -> str:
    """One-line, length-capped summary of `exc`'s message, safe to show in a public PR comment.

    The build/run/classify exceptions this tool raises (`ParamsBuildError`,
    `ModelRunError`, `ClassifyError`, all built via `run_checked` above)
    carry the full failed command, its cwd, and its complete captured
    stdout/stderr as their message -- exactly right for a CI job log, far
    too much to paste into a public PR comment verbatim, especially
    multiplied across every failing file in a large corpus. R and Python
    both conventionally put the actual error text on the last substantive
    line of their output (R's crash trailer -- "Calls: ...", "Execution
    halted" -- comes after it; a Python traceback's last line already *is*
    it), so this strips known boilerplate lines and returns the last
    surviving one, capped to `max_len` characters. A maintainer gets enough
    to recognise and reproduce the failure locally without a multi-KB dump
    landing in the comment; the full, uncapped message is still available
    wherever the exception itself is logged (stderr/CI logs, never a public
    comment).
    """
    lines = [line.strip() for line in str(exc).splitlines() if line.strip()]
    useful = [line for line in lines if not line.startswith(_BOILERPLATE_LINE_PREFIXES)]
    reason = useful[-1] if useful else "unknown error"
    if len(reason) > max_len:
        reason = reason[: max_len - 1].rstrip() + "…"
    return reason
