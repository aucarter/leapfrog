# leapfrog-validate

Compare leapfrog model output across git refs, and (eventually) against
Spectrum. See `.scratch/leapfrog-validation/PRD.md` for the full design;
this package currently implements the walking skeleton
(`.scratch/leapfrog-validation/issues/15-walking-skeleton-single-indicator-diff.md`),
the full five-indicator registry and exclusion mechanism
(`.scratch/leapfrog-validation/issues/17-full-indicator-registry-tolerance-rollup.md`),
the `compare` directory wrapper plus working-tree support
(`.scratch/leapfrog-validation/issues/19-compare-wrapper-working-tree.md`),
and `compare`'s private Azure Blob corpus source plus tag-based indicator
scoping
(`.scratch/leapfrog-validation/issues/20-wire-corpus-into-compare-tag-scoping.md`).

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
