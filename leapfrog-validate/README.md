# leapfrog-validate

Check that leapfrog model output stays correct. The package compares
leapfrog with an earlier leapfrog build, and with Spectrum's own values.
See `.scratch/leapfrog-validation/PRD.md` for the full design. The sections
below use "PJNZ file" for one input file and "indicator" for one output
that the tool compares.

## What it does

| Feature | What you get |
|---|---|
| Regression check | Compares a PR with `main` on every PJNZ file in the corpus. Tight tolerance. Meant to block merges. |
| Spectrum check | Compares leapfrog with the values stored in each PJNZ file. Looser tolerance. Never blocks a merge. |
| PR comments | One comment for each check. Shows the pass rate, then collapsed lists of passed and failed files. Each failed file opens to its indicators and a reproduce command. Comments show counts and file names, never real numbers. |
| Indicators | `total_population`, `hiv_population`, `treatment_population`, `aids_deaths_single_age`, `aids_deaths_on_treatment`. All by age and sex. The Spectrum check also has `pmtct_need` and `cotrim_need`. |
| Tolerance | Each cell passes if `\|a - b\| <= atol + rtol * \|a\|`. One failed cell fails the indicator. |
| Exclusions | Skip one known, explained difference for one PJNZ file. Needs a reason and a link. |
| Tags | Tags found from the PJNZ file (`goals`, `aim_only`, `has_pmtct`, `has_cotrim`). A tag can limit an indicator to some files. |
| Refs | Any git branch or SHA, or `working-tree` for your uncommitted changes. Builds are cached. |
| Private corpus | PJNZ files in Azure Blob storage. CI logs in with GitHub OIDC. No PJNZ data is in this repo. |
| Local commands | `build-params`, `run`, `diff`, `compare` and `classify`. See "Usage". |

Only `leapfrogr` runs the model here. `build-params` calls
`leapfrog::process_pjnz()`. `run` calls `leapfrog::run_model()`. The R scripts
are in `src/leapfrog_validate/r_scripts/`. Everything else is Python.

A separate check, `.github/workflows/model-output-equality.yaml`, compares
the R, Python and C++ interfaces with exact `h5diff`. It covers the
`leapfrog-py` and C++ adapters. Keep it.

## How to change things

All paths are under `src/leapfrog_validate/`.

| I want to... | Edit this |
|---|---|
| Add an indicator to the regression check | `INDICATORS` in `indicators.py` |
| Add an indicator to the Spectrum check | `SPECTRUM_INDICATORS` in `spectrum_indicators.py` |
| Change a tolerance | `Tolerance(...)` in the indicator entry. Regression: `RTOL` in `indicators.py`. Spectrum: `SPECTRUM_RTOL` and `SPECTRUM_ATOL` in `spectrum_indicators.py`. |
| Exclude a known difference | `exclusions` tuple in the indicator entry |
| Limit an indicator to some files | `scope` in the indicator entry |
| Add a tag found from the file | `classify.py` and `r_scripts/classify_pjnz.R` |
| Add a tag by hand | A manifest file (see "Manifest") |
| Change how Spectrum values are read | `spectrum_extract.py` |
| Change the PR comment text or layout | `report_regression.py`, `report_spectrum.py`, `comment_format.py` |
| Change when CI runs | `.github/workflows/regression-track.yml`, `spectrum-track.yml` |
| Add a PJNZ file to the corpus | Upload it to the Azure container. See `infra/README.md`. No code change. |
| Change which files the R, Python and C++ check uses | `select_fixtures.py` and `model-output-equality.yaml` |

The regression check and the Spectrum check have **separate** registries.
An indicator in one is not in the other.

### Add an indicator

1. Write an `extract_...` function in `indicators.py`. It takes the path of
   an `output.h5` file and returns a NumPy array.
2. For the Spectrum check, also write an `extract_spectrum_...` function in
   `spectrum_extract.py`. It returns an array of the same shape and
   covers the same years.
3. Add an entry to the registry (or both registries). Set `tolerance`,
   `exclusions` (use `()` if none) and `scope` (use `ALL` for every file).
4. Add a test in `tests/`.

The `diff` command, the CI jobs and the PR comments read the registry. You
do not need to change any other list. `pmtct_need` in
`spectrum_indicators.py` is a small working example.

### Add an exclusion

Add an `Exclusion` to the `exclusions` tuple of the indicator:

```python
Exclusion(
    pjnz="ETH/example.PJNZ",
    reason="Short reason",
    link="https://github.com/hivtools/leapfrog/issues/123",
    year=(40, 50),  # optional. Index range, both ends included.
    sex=None,       # optional. None means every index.
    age=(0, 14),    # optional.
)
```

`reason` and `link` are required. Leave out `year`, `sex` and `age` to
exclude the whole indicator for that file. The bounds are index positions,
not calendar years. Year 0 is 1970. Sex 0 is male and sex 1 is female. Age
is single years, 0 to 80. A year-only indicator (`pmtct_need`) has no sex or
age, so an entry with a `sex` or `age` bound is rejected for it.

The `pjnz` text must match **exactly**. The two checks use different text:

- **Spectrum check:** the corpus path with the extension, for example `ETH/example.PJNZ`.
- **Regression check:** the corpus path without the extension, and with `/` changed to `__`, for example `ETH__example`. A file in the top folder is just `example`.

A text that does not match is ignored with no error. So add a test that
proves the exclusion applies. For the regression check, you can also run
`leapfrog-validate diff a.h5 b.h5 --pjnz "ETH__example"` and look for
`N excluded` in the output.

### Tags and the manifest

Tags come from three places:

- `goals` or `aim_only`: from the zip contents (a `.HV` member means `goals`).
- `has_pmtct` and `has_cotrim`: from the input data. The tag is true when the PMTCT or cotrimoxazole input array is not all zero.
- The manifest: tags you set by hand.

To see the tags of one file, run `uv run leapfrog-validate classify <ref> <file.PJNZ>`.

To limit an indicator to some files, set `scope=tag("has_pmtct")`. A file
must have every listed tag. A file without the tag is skipped with no error.

**Manifest.** This is a JSON file, for example
`{"ETH/example.PJNZ": ["custom_made"]}`. Pass it with `--manifest` to
`compare` or `classify`. No manifest file exists in the repo today. **The CI
jobs do not read a manifest.** To use manifest tags in CI, add a file to the
repo and change the workflows and the CI scripts.

### Run the tests

```sh
uv run pytest                       # whole suite. About 6 minutes. Builds leapfrogr.
uv run pytest tests/test_report_spectrum.py   # one file. Seconds.
```

## Usage

```sh
uv run leapfrog-validate build-params <ref> <pjnz> -o params.h5
uv run leapfrog-validate run <ref> params.h5 -o output.h5
uv run leapfrog-validate diff output-a.h5 output-b.h5
uv run leapfrog-validate compare <ref> <candidate> --pjnz-dir <dir>
```

Each command independently builds `leapfrogr` (and regenerates the C++
headers it depends on) at the given git ref, in an isolated git worktree
cached under `--cache-dir` (default `~/.cache/leapfrog-validate`; note this
is a global option and must come *before* the subcommand, e.g.
`leapfrog-validate --cache-dir /tmp/cache compare ...`).

`<ref>` accepts a committed git ref/SHA, or the literal value `working-tree`
for your own current uncommitted checkout -- so you can check "how does my
in-progress change compare to main" without committing first. A working-tree
build is cached by a content hash of `leapfrog-core/`, `codegen/`, and
`leapfrogr/` (the directories that actually feed the build) rather than a
git SHA, so re-running against the same uncommitted state reuses the cached
build instead of rebuilding.

### `compare`: running the full pipeline across a directory of PJNZ files

```sh
uv run leapfrog-validate compare main working-tree --pjnz-dir ./pjnz-corpus
```

Builds `<ref>` and `<candidate>` once each, then runs `build-params` → `run`
→ `diff` (every applicable indicator -- all five blessed ones, plus any
tag-scoped custom indicator whose tag the file carries, see "Tag-scoped
indicators" below) for every `*.PJNZ` file found, printing a per-indicator
verdict and a per-file PASS/FAIL line, plus a summary at the end. Exits
non-zero if any file fails any applicable indicator. `--pjnz-dir` (a local
directory, recursive) and `--azure-container`/`--azure-account` (the real
private corpus) are mutually exclusive alternatives for where those PJNZ
come from -- see "running against the private Azure Blob corpus" below.

`diff` applies a hybrid `atol + rtol*|ref|` per-cell tolerance (strict-max
rollup: any single over-tolerance cell fails that indicator) and, by
default, checks all five blessed indicators against two `output.h5`
artifacts -- `total_population`, `hiv_population`, `treatment_population`,
`aids_deaths_single_age`, `aids_deaths_on_treatment` -- printing one
pass/fail line per indicator and exiting non-zero if any of them fail.
Pass `--indicator <name>` to check just one.

`treatment_population` and `aids_deaths_on_treatment` have no single
leapfrog output array; they're reconstructed by summing the adult and
pediatric ART-population/ART-death arrays over CD4 stage and treatment
duration and concatenating the three age domains (see
`leapfrog_validate.indicators`).

### `compare`: running against the private Azure Blob corpus

```sh
uv run leapfrog-validate compare main working-tree \
  --azure-container pjnz-archive --azure-account pjnzarchive
```

Lists and downloads every `.PJNZ` blob under `--azure-prefix` (default:
`"2026 Estimates/Public Spectrum files"`, the real corpus's path per
`leapfrog-validate/infra/README.md`) into a temp dir, preserving subfolder
structure (e.g. the corpus's `ETH/` subfolder), then runs the same
build-params → run → diff pipeline against it. `--azure-container`/
`--azure-account` also read from the `AZURE_STORAGE_CONTAINER`/
`AZURE_STORAGE_ACCOUNT` environment variables -- the same names
`infra/deploy.sh` publishes as GitHub repo variables. Authenticates via
`azure.identity.DefaultAzureCredential` (an `az login`'d session locally, or
a GitHub-OIDC-federated managed identity in CI) -- no static secret either
way.

### Tag-scoped indicators

An indicator's `IndicatorSpec` carries a `scope: Scope` (see
`leapfrog_validate.indicators`): `ALL` (the default for all five blessed
indicators -- every PJNZ is in scope, unchanged behavior) or
`tag("some_tag", ...)`, restricting it to PJNZ carrying every listed tag.
Tags come from `leapfrog_validate.classify.classify` (shape + domain +
optional `--manifest` tags); `compare` checks each PJNZ's tags against every
indicator's scope and skips (reporting a `SKIP` line, not silence) any
indicator that doesn't apply -- so adding a new, correctly-tagged PJNZ to the
corpus makes a tag-scoped indicator eligible for it automatically, no code
or config change required. Tags are only computed at all if at least one
registered indicator actually has a non-`ALL` scope, so the five blessed
indicators alone never pay for the (Rscript-shelling) classify call.

### Excluding a known, explained discrepancy

Pass `--pjnz <identifier>` to have `diff` also check each indicator's
exclusion list (`leapfrog_validate.exclusions.Exclusion`) for entries
scoped to that PJNZ, carving out the matching (year, sex, age) cells from
the pass/fail rollup -- visible in the summary line as `N excluded` --
without loosening tolerance for any other PJNZ. Every `Exclusion` requires
both a `reason` and a `link` to the underlying explanation; constructing
one without either raises `ValueError`.
