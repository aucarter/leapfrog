"""Tests for `compare`'s Azure Blob corpus-sourcing option (ticket 20).

Ticket 16 amended the private PJNZ corpus's hosting from a planned git
submodule to Azure Blob Storage mid-build (see its Comments and
`leapfrog-validate/infra/README.md`) -- `compare` needs to be able to list
and download matching blobs into a temp dir, then reuse the same recursive
PJNZ walk a local `--pjnz-dir` already gets.

This sandbox has no Azure credentials and no network path to a real
storage account, so everything here runs against a fake, duck-typed
blob-service/container client -- `_azure_blob_service_client` (the one spot
that would construct a real `azure.storage.blob.BlobServiceClient`) is
monkeypatched out in every test, never exercised for real. See ticket 20's
Comments for this limitation, mirroring how ticket 16 flagged its own
Bicep-not-compile-checked-against-a-live-subscription limitation.
"""

from pathlib import Path

import h5py
import numpy as np
import pytest
from typer.testing import CliRunner

from leapfrog_validate import cli
from leapfrog_validate.build import BuildWorkspace
from leapfrog_validate.cli import _download_azure_pjnz_corpus, app

runner = CliRunner()

AZURE_CORPUS_PREFIX = "2026 Estimates/Public Spectrum files"


class _FakeBlob:
    def __init__(self, name: str) -> None:
        self.name = name


class _FakeDownload:
    def __init__(self, data: bytes) -> None:
        self._data = data

    def readinto(self, stream) -> None:
        stream.write(self._data)


class _FakeContainerClient:
    def __init__(self, blobs: dict[str, bytes]) -> None:
        self._blobs = blobs

    def list_blobs(self, name_starts_with: str = ""):
        return [_FakeBlob(name) for name in sorted(self._blobs) if name.startswith(name_starts_with)]

    def download_blob(self, name: str) -> _FakeDownload:
        return _FakeDownload(self._blobs[name])


class _FakeCredential:
    def __init__(self) -> None:
        self.closed = False

    def close(self) -> None:
        self.closed = True


class _FakeBlobServiceClient:
    def __init__(self, blobs: dict[str, bytes]) -> None:
        self._blobs = blobs
        self.credential = _FakeCredential()
        self.closed = False

    def get_container_client(self, container: str) -> _FakeContainerClient:  # noqa: ARG002
        return _FakeContainerClient(self._blobs)

    def close(self) -> None:
        self.closed = True


class TestDownloadAzurePjnzCorpus:
    """Unit tests for the download helper itself, no CLI involved."""

    def test_downloads_only_pjnz_blobs_under_the_prefix(self, tmp_path):
        blobs = {
            f"{AZURE_CORPUS_PREFIX}/kenya.PJNZ": b"kenya-bytes",
            f"{AZURE_CORPUS_PREFIX}/readme.txt": b"not a pjnz",
            "2026 Estimates/Other files/other.PJNZ": b"wrong prefix",
        }
        client = _FakeBlobServiceClient(blobs)

        downloaded = _download_azure_pjnz_corpus(client, "pjnz-archive", AZURE_CORPUS_PREFIX, tmp_path)

        assert [p.relative_to(tmp_path).as_posix() for p in downloaded] == ["kenya.PJNZ"]
        assert (tmp_path / "kenya.PJNZ").read_bytes() == b"kenya-bytes"

    def test_preserves_subfolder_structure_eg_eth(self, tmp_path):
        """The real corpus has an `ETH/` subfolder (ticket 16's infra/README.md) -- not flattened."""
        blobs = {
            f"{AZURE_CORPUS_PREFIX}/kenya.PJNZ": b"kenya-bytes",
            f"{AZURE_CORPUS_PREFIX}/ETH/addis.PJNZ": b"addis-bytes",
        }
        client = _FakeBlobServiceClient(blobs)

        downloaded = _download_azure_pjnz_corpus(client, "pjnz-archive", AZURE_CORPUS_PREFIX, tmp_path)

        relative = sorted(p.relative_to(tmp_path).as_posix() for p in downloaded)
        assert relative == ["ETH/addis.PJNZ", "kenya.PJNZ"]
        assert (tmp_path / "ETH" / "addis.PJNZ").read_bytes() == b"addis-bytes"

    def test_prefix_without_trailing_slash_does_not_match_a_sibling_prefix(self, tmp_path):
        """A prefix of "foo" must not also match blobs under "foo2/" -- boundary must be a path separator."""
        blobs = {
            "corpus/kenya.PJNZ": b"real",
            "corpus2/other.PJNZ": b"should not match",
        }
        client = _FakeBlobServiceClient(blobs)

        downloaded = _download_azure_pjnz_corpus(client, "pjnz-archive", "corpus", tmp_path)

        assert [p.relative_to(tmp_path).as_posix() for p in downloaded] == ["kenya.PJNZ"]

    def test_empty_corpus_returns_no_files(self, tmp_path):
        client = _FakeBlobServiceClient({})

        downloaded = _download_azure_pjnz_corpus(client, "pjnz-archive", AZURE_CORPUS_PREFIX, tmp_path)

        assert downloaded == []

    def test_empty_prefix_matches_every_blob_rather_than_none(self, tmp_path):
        """An empty prefix means "no filter" -- a naive trailing-slash join would instead match nothing."""
        blobs = {"top-level.PJNZ": b"real", "nested/sub.PJNZ": b"also real"}
        client = _FakeBlobServiceClient(blobs)

        downloaded = _download_azure_pjnz_corpus(client, "pjnz-archive", "", tmp_path)

        relative = sorted(p.relative_to(tmp_path).as_posix() for p in downloaded)
        assert relative == ["nested/sub.PJNZ", "top-level.PJNZ"]

    def test_rejects_a_blob_name_that_would_escape_dest_dir_via_a_leading_slash(self, tmp_path):
        """A blob named `<prefix>//escape.PJNZ` makes the stripped tail start with "/".

        `dest_dir / "/escape.PJNZ"` evaluates to the bare absolute path (Python's
        `Path.__truediv__` discards the left side for an absolute right side) --
        writing outside `dest_dir` entirely. Must be rejected, not silently
        followed.
        """
        blobs = {f"{AZURE_CORPUS_PREFIX}//escape.PJNZ": b"malicious"}
        client = _FakeBlobServiceClient(blobs)

        with pytest.raises(cli.CorpusError, match=r"escape\.PJNZ"):
            _download_azure_pjnz_corpus(client, "pjnz-archive", AZURE_CORPUS_PREFIX, tmp_path)

        assert not (tmp_path.parent / "escape.PJNZ").exists()

    def test_rejects_a_blob_name_with_a_parent_directory_traversal_segment(self, tmp_path):
        blobs = {f"{AZURE_CORPUS_PREFIX}/../escape.PJNZ": b"malicious"}
        client = _FakeBlobServiceClient(blobs)

        with pytest.raises(cli.CorpusError, match=r"escape\.PJNZ"):
            _download_azure_pjnz_corpus(client, "pjnz-archive", AZURE_CORPUS_PREFIX, tmp_path)

        assert not (tmp_path.parent / "escape.PJNZ").exists()


@pytest.fixture
def fake_azure_and_build(monkeypatch):
    """Mock `_azure_blob_service_client` (never touch real Azure) plus build/run, like test_cli_compare.py."""

    def _write_output_h5(path: Path, scale: float) -> None:
        shape = (2, 2, 9)
        art_shape = (2, 2, 4, 7, 3)
        child_shape = (2, 2, 2, 7, 3)
        with h5py.File(path, "w") as f:
            f.create_dataset("p_totpop", data=np.full(shape, 100.0 * scale))
            f.create_dataset("p_hivpop", data=np.full(shape, 10.0 * scale))
            f.create_dataset("p_hiv_deaths", data=np.full(shape, 1.0 * scale))
            f.create_dataset("h_artpop", data=np.full(art_shape, 1.0 * scale))
            f.create_dataset("hc1_artpop", data=np.full(child_shape, 1.0 * scale))
            f.create_dataset("hc2_artpop", data=np.full(child_shape, 1.0 * scale))
            f.create_dataset("h_hiv_deaths_art", data=np.full(art_shape, 0.1 * scale))
            f.create_dataset("hc1_art_aids_deaths", data=np.full(child_shape, 0.1 * scale))
            f.create_dataset("hc2_art_aids_deaths", data=np.full(child_shape, 0.1 * scale))

    def fake_prepare_ref(repo_root: Path, cache_dir: Path, ref: str) -> BuildWorkspace:
        del repo_root, cache_dir
        worktree = Path(f"/fake/{ref}")
        return BuildWorkspace(worktree=worktree, r_library=worktree / "r-library")

    def fake_build_params(workspace: BuildWorkspace, pjnz: Path, output: Path) -> None:
        del workspace, pjnz, output

    def fake_run_model(workspace: BuildWorkspace, params_path: Path, output: Path, configuration: str) -> None:
        del configuration, workspace
        side = "candidate" if params_path.stem.endswith("-candidate-params") else "ref"
        _write_output_h5(output, scale=1.0 if side == "ref" else scales_holder["scale"])

    scales_holder = {"scale": 1.0}
    monkeypatch.setattr(cli, "_prepare_ref", fake_prepare_ref)
    monkeypatch.setattr(cli.params, "build_params", fake_build_params)
    monkeypatch.setattr(cli.model_run, "run_model", fake_run_model)
    return scales_holder


def test_compare_against_azure_container_downloads_and_runs(fake_azure_and_build, monkeypatch):
    del fake_azure_and_build  # fixture needed only for its _prepare_ref/build/run monkeypatching side effect
    blobs = {f"{AZURE_CORPUS_PREFIX}/kenya.PJNZ": b"fake-pjnz-bytes"}
    monkeypatch.setattr(cli, "_azure_blob_service_client", lambda account: _FakeBlobServiceClient(blobs))  # noqa: ARG005

    result = runner.invoke(
        app,
        [
            "compare",
            "main",
            "candidate-branch",
            "--azure-container",
            "pjnz-archive",
            "--azure-account",
            "pjnzarchive",
        ],
    )

    assert result.exit_code == 0, result.output
    assert "PASS  kenya.PJNZ" in result.output


def test_compare_against_azure_container_flags_a_real_regression(fake_azure_and_build, monkeypatch):
    fake_azure_and_build["scale"] = 1.5
    blobs = {f"{AZURE_CORPUS_PREFIX}/kenya.PJNZ": b"fake-pjnz-bytes"}
    monkeypatch.setattr(cli, "_azure_blob_service_client", lambda account: _FakeBlobServiceClient(blobs))  # noqa: ARG005

    result = runner.invoke(
        app,
        ["compare", "main", "candidate-branch", "--azure-container", "pjnz-archive", "--azure-account", "pjnzarchive"],
    )

    assert result.exit_code == 1
    assert "FAIL  kenya.PJNZ" in result.output


def test_compare_rejects_both_pjnz_dir_and_azure_container(tmp_path):
    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()

    result = runner.invoke(
        app,
        [
            "compare",
            "main",
            "candidate-branch",
            "--pjnz-dir",
            str(pjnz_dir),
            "--azure-container",
            "pjnz-archive",
            "--azure-account",
            "pjnzarchive",
        ],
    )

    assert result.exit_code == 2
    assert "exactly one of" in result.output


def test_compare_rejects_neither_pjnz_dir_nor_azure_container():
    result = runner.invoke(app, ["compare", "main", "candidate-branch"])

    assert result.exit_code == 2
    assert "exactly one of" in result.output


def test_compare_rejects_azure_container_without_azure_account():
    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--azure-container", "pjnz-archive"])

    assert result.exit_code == 2
    assert "azure-account" in result.output


def test_compare_pjnz_dir_is_not_broken_by_ambient_azure_env_vars(tmp_path, monkeypatch):
    """An explicit `--pjnz-dir` must win even if AZURE_STORAGE_* happen to be exported.

    `infra/deploy.sh` publishes `AZURE_STORAGE_CONTAINER`/`AZURE_STORAGE_ACCOUNT` as GitHub
    repo variables -- any CI job (or local shell) where those are exported for some other,
    unrelated step must not break a plain `--pjnz-dir` invocation in the same job/shell.
    """
    monkeypatch.setenv("AZURE_STORAGE_CONTAINER", "pjnz-archive")
    monkeypatch.setenv("AZURE_STORAGE_ACCOUNT", "pjnzarchive")
    pjnz_dir = tmp_path / "corpus"
    pjnz_dir.mkdir()
    (pjnz_dir / "a.PJNZ").touch()

    result = runner.invoke(app, ["compare", "main", "candidate-branch", "--pjnz-dir", str(pjnz_dir)])

    assert "exactly one of" not in result.output
    assert result.exit_code != 2


def test_compare_falls_back_to_azure_env_vars_when_neither_option_is_given(fake_azure_and_build, monkeypatch):
    """With neither `--pjnz-dir` nor `--azure-container` given, env vars become the source."""
    del fake_azure_and_build
    blobs = {f"{AZURE_CORPUS_PREFIX}/kenya.PJNZ": b"fake-pjnz-bytes"}
    monkeypatch.setattr(cli, "_azure_blob_service_client", lambda account: _FakeBlobServiceClient(blobs))  # noqa: ARG005
    monkeypatch.setenv("AZURE_STORAGE_CONTAINER", "pjnz-archive")
    monkeypatch.setenv("AZURE_STORAGE_ACCOUNT", "pjnzarchive")

    result = runner.invoke(app, ["compare", "main", "candidate-branch"])

    assert result.exit_code == 0, result.output
    assert "PASS  kenya.PJNZ" in result.output


def test_compare_closes_the_azure_blob_service_client_and_credential(fake_azure_and_build, monkeypatch):
    """The SDK client (and the credential it holds) must be closed after use, not leaked.

    Low severity for a short-lived CLI invocation (the process exits right
    after), but inconsistent with azure-identity/azure-storage-blob's
    documented context-manager usage pattern, and would matter for a
    long-running job processing many containers/files.
    """
    del fake_azure_and_build
    blobs = {f"{AZURE_CORPUS_PREFIX}/kenya.PJNZ": b"fake-pjnz-bytes"}
    fake_client = _FakeBlobServiceClient(blobs)
    monkeypatch.setattr(cli, "_azure_blob_service_client", lambda account: fake_client)  # noqa: ARG005

    result = runner.invoke(
        app,
        ["compare", "main", "candidate-branch", "--azure-container", "pjnz-archive", "--azure-account", "pjnzarchive"],
    )

    assert result.exit_code == 0, result.output
    assert fake_client.closed, "BlobServiceClient was never closed"
    assert fake_client.credential.closed, "the client's credential was never closed"


def test_compare_reports_an_unsafe_blob_name_as_a_clean_error_not_a_crash(fake_azure_and_build, monkeypatch):
    """A `CorpusError` from the download step must surface as a clean CLI error, not a traceback."""
    del fake_azure_and_build
    blobs = {f"{AZURE_CORPUS_PREFIX}//escape.PJNZ": b"malicious"}
    monkeypatch.setattr(cli, "_azure_blob_service_client", lambda account: _FakeBlobServiceClient(blobs))  # noqa: ARG005

    result = runner.invoke(
        app,
        ["compare", "main", "candidate-branch", "--azure-container", "pjnz-archive", "--azure-account", "pjnzarchive"],
    )

    assert not isinstance(result.exception, cli.CorpusError), (
        f"CorpusError leaked out uncaught instead of being reported cleanly: {result.exception!r}"
    )
    assert result.exit_code == 2
    assert "unsafe" in result.output
