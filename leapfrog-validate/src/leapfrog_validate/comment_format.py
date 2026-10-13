"""Shared markdown helpers for the two sticky PR comments.

Both comments (`report_regression`, `report_spectrum`) show failed files as
nested, collapsed GitHub `<details>` blocks:

    Failed files (x/N)          <- closed by default
      `file.PJNZ` -- a/b ...    <- closed by default
        indicator ticks/crosses
        Reproduce this          <- closed by default

Pure functions only: no I/O, and no numbers other than counts the caller
passes in.

GitHub renders markdown inside `<details>` only if a blank line separates
the HTML tags from the markdown. It also turns a line indented by four or
more spaces into a code block. So every line here starts at column 0, and
nesting depth comes from the tags, never from indentation.
"""

from __future__ import annotations

import html
from collections.abc import Iterable


def code_label(text: str) -> str:
    """Render `text` as inline code that is safe inside a `<summary>` tag.

    Markdown backticks are not reliable inside `<summary>`, so use HTML.
    """
    return f"<code>{html.escape(text)}</code>"


def details(summary_html: str, body: Iterable[str]) -> list[str]:
    """Return a closed `<details>` block as lines, with `body` as its markdown content.

    `summary_html` goes into the `<summary>` tag as-is: the caller escapes
    it (use `code_label` for file names).
    """
    return [
        "<details>",
        f"<summary>{summary_html}</summary>",
        "",
        *body,
        "",
        "</details>",
    ]


def reproduce_block(commands: Iterable[str]) -> list[str]:
    """Return the closed "Reproduce this" block holding a bash code fence."""
    return details("Reproduce this", ["```bash", *commands, "```"])
