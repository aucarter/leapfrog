"""Tests for leapfrog_validate.subprocess_utils.summarize_error_reason."""

from leapfrog_validate.subprocess_utils import summarize_error_reason


def test_extracts_the_r_error_line_from_a_run_checked_style_message():
    exc = RuntimeError(
        "classify-pjnz failed (exit 1)\n"
        "command: Rscript classify_pjnz.R\n"
        "cwd: /tmp/worktree\n"
        "--- stdout ---\n"
        "\n"
        "--- stderr ---\n"
        "Error in !is.null(pars$x) && !is.na(pars$x) : \n"
        "  'length = 9' in coercion to 'logical(1)'\n"
        "Calls: <Anonymous> -> process_pjnz_ha\n"
        "Execution halted"
    )
    assert summarize_error_reason(exc) == "'length = 9' in coercion to 'logical(1)'"


def test_extracts_the_last_line_of_a_plain_python_exception():
    exc = RuntimeError("Could not import Spectrum modvars from foo.PJNZ: unsupported operand type(s) for +")
    assert summarize_error_reason(exc) == (
        "Could not import Spectrum modvars from foo.PJNZ: unsupported operand type(s) for +"
    )


def test_falls_back_to_a_placeholder_for_an_empty_message():
    assert summarize_error_reason(RuntimeError("")) == "unknown error"


def test_caps_length_with_an_ellipsis():
    exc = RuntimeError("x" * 500)
    reason = summarize_error_reason(exc, max_len=50)
    assert len(reason) == 50
    assert reason.endswith("…")


def test_strips_the_calls_and_execution_halted_trailer():
    exc = RuntimeError(
        "model-run failed (exit 1)\n"
        "command: Rscript run_model.R\n"
        "cwd: /tmp/worktree\n"
        "--- stdout ---\n"
        "\n"
        "--- stderr ---\n"
        "Error: something broke\n"
        "Calls: run_model -> stop\n"
        "Execution halted"
    )
    assert summarize_error_reason(exc) == "Error: something broke"
