"""Shared structural assertions for the sticky-comment markdown (regression and Spectrum tracks)."""

import re


def assert_nested_collapsed_failures(markdown: str, failed_summary: str, failed_label: str) -> None:
    """Assert `markdown` has the layout: Failed files > one block per file > Reproduce this.

    All three levels are closed by default (no `open` attribute), and the
    `<details>` tags balance. No line starts with four spaces, because
    GitHub would render that as a code block instead of markdown.
    """
    assert "<details open" not in markdown
    assert markdown.count("<details>") == markdown.count("</details>")
    assert not re.search(r"^ {4}", markdown, re.M)

    failed_pos = markdown.index(failed_summary)
    file_pos = markdown.index(f"<summary><code>{failed_label}</code>", failed_pos)
    reproduce_pos = markdown.index("<summary>Reproduce this</summary>", file_pos)
    assert failed_pos < file_pos < reproduce_pos

    # The opening tag before each summary is a bare, closed `<details>`.
    for pos in (failed_pos, file_pos, reproduce_pos):
        assert markdown[:pos].rstrip().endswith("<details>")
    # The blank line after `</summary>` lets GitHub render markdown inside.
    assert re.search(r"</summary>\n\n", markdown[failed_pos:])
