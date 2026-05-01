from __future__ import annotations

import csv
import hashlib
import os
import subprocess
import sys
from pathlib import Path

import pytest


FICTRAC_ROOT = Path(__file__).resolve().parents[1]
FICTRAC_BIN = FICTRAC_ROOT / "bin" / "Release" / "fictrac.exe"
FICTRAC_SAMPLE_CONFIG = FICTRAC_ROOT / "sample" / "config.txt"
FICTRAC_SAMPLE_VIDEO = FICTRAC_ROOT / "sample" / "sample.mp4"

CONFLICTING_ENV_VARS = (
    "CONDA_DEFAULT_ENV",
    "CONDA_PREFIX",
    "CONDA_PROMPT_MODIFIER",
    "CONDA_EXE",
    "CONDA_PYTHON_EXE",
    "CONDA_SHLVL",
    "PYTHONHOME",
    "PYTHONPATH",
    "PYTHONEXECUTABLE",
    "VIRTUAL_ENV",
)

# Native offline regression contract for the bundled sample video. Columns 22,
# 24, and 25 in the `.dat` output are runtime-dependent timestamps, so the
# parity signature covers only the deterministic tracking fields.
PARITY_EXPECTED_ROWS = 300
PARITY_EXPECTED_SHA256 = "4e5c692c0754b45de4d61d24002cbc8b21eec1125f4bc9e53a4a45b1bbcc6307"
PARITY_IGNORED_ZERO_BASED_COLUMNS = (21, 23, 24)
PARITY_KEPT_ZERO_BASED_COLUMNS = tuple(
    index for index in range(25) if index not in PARITY_IGNORED_ZERO_BASED_COLUMNS
)


def _require_sample_parity_assets() -> None:
    if os.name != "nt":
        pytest.skip("Native FicTrac sample parity is validated on Windows")
    if not FICTRAC_BIN.exists():
        pytest.skip(f"FicTrac binary not found: {FICTRAC_BIN}")
    if not FICTRAC_SAMPLE_CONFIG.exists():
        pytest.skip(f"FicTrac sample config not found: {FICTRAC_SAMPLE_CONFIG}")
    if not FICTRAC_SAMPLE_VIDEO.exists():
        pytest.skip(f"FicTrac sample video not found: {FICTRAC_SAMPLE_VIDEO}")


def _split_env_paths(value: str | None) -> list[Path]:
    if not value:
        return []
    return [Path(part) for part in value.split(os.pathsep) if part.strip()]


def _candidate_runtime_dirs() -> list[Path]:
    roots = []
    roots.extend(_split_env_paths(os.environ.get("SPINNAKER_ROOT")))
    roots.extend(_split_env_paths(os.environ.get("PGR_DIR")))
    roots.extend(_split_env_paths(os.environ.get("FLYCAPTURE_ROOT")))

    candidates: list[Path] = []
    for root in roots:
        candidates.append(root)
        candidates.append(root / "bin64" / "vs2015")

    candidates.extend(
        [
            Path(r"C:\Program Files\Teledyne\Spinnaker\bin64\vs2015"),
            Path(r"C:\Program Files\Point Grey Research\FlyCapture2\bin64\vs2015"),
        ]
    )

    ordered: list[Path] = []
    seen: set[str] = set()
    for candidate in candidates:
        normalized = os.path.normcase(str(candidate))
        if normalized in seen:
            continue
        seen.add(normalized)
        if candidate.is_dir():
            ordered.append(candidate)
    return ordered


def _is_within_root(path_str: str, root: Path) -> bool:
    try:
        candidate = Path(path_str).resolve(strict=False)
        root_resolved = root.resolve(strict=False)
    except OSError:
        return False

    try:
        common = os.path.commonpath([str(candidate), str(root_resolved)])
    except ValueError:
        return False
    return os.path.normcase(common) == os.path.normcase(str(root_resolved))


def _build_subprocess_env() -> dict[str, str]:
    env = dict(os.environ)
    for key in CONFLICTING_ENV_VARS:
        env.pop(key, None)

    blocked_roots = [Path(sys.executable).resolve(strict=False).parent]
    conda_prefix = os.environ.get("CONDA_PREFIX")
    if conda_prefix:
        blocked_roots.append(Path(conda_prefix))

    path_parts = [part for part in env.get("PATH", "").split(os.pathsep) if part.strip()]
    prepend_candidates = [str(FICTRAC_BIN.parent), *(str(path) for path in _candidate_runtime_dirs())]

    filtered: list[str] = []
    seen: set[str] = set()
    for part in [*prepend_candidates, *path_parts]:
        normalized = os.path.normcase(part)
        if normalized in seen:
            continue
        if any(_is_within_root(part, root) for root in blocked_roots):
            continue
        seen.add(normalized)
        filtered.append(part)

    env["PATH"] = os.pathsep.join(filtered)
    return env


def _upsert_config_line(lines: list[str], key: str, value: str) -> list[str]:
    prefix = f"{key:<16} :"
    replacement = f"{prefix} {value}\n"
    for index, line in enumerate(lines):
        if line.split(":", 1)[0].strip() == key:
            lines[index] = replacement
            return lines
    lines.append(replacement)
    return lines


def _build_sample_runtime_config(tmp_path: Path) -> tuple[Path, Path]:
    runtime_config = tmp_path / "fictrac_sample_runtime_config.txt"
    output_base = tmp_path / "sample_parity"
    lines = FICTRAC_SAMPLE_CONFIG.read_text(encoding="utf-8").splitlines(keepends=True)
    lines = _upsert_config_line(lines, "src_fn", FICTRAC_SAMPLE_VIDEO.as_posix())
    lines = _upsert_config_line(lines, "output_fn", output_base.as_posix())
    lines = _upsert_config_line(lines, "do_display", "n")
    lines = _upsert_config_line(lines, "save_debug", "n")
    lines = _upsert_config_line(lines, "save_raw", "n")
    runtime_config.write_text("".join(lines), encoding="utf-8")
    return runtime_config, output_base


def _run_sample_parity(tmp_path: Path) -> Path:
    runtime_config, output_base = _build_sample_runtime_config(tmp_path)
    completed = subprocess.run(
        [str(FICTRAC_BIN), str(runtime_config), "--verbosity", "warn"],
        cwd=str(FICTRAC_ROOT),
        env=_build_subprocess_env(),
        capture_output=True,
        text=True,
        timeout=120,
        check=False,
    )
    assert completed.returncode == 0, (
        "Sample FicTrac parity run failed\n"
        f"stdout:\n{completed.stdout}\n"
        f"stderr:\n{completed.stderr}"
    )

    outputs = sorted(tmp_path.glob(f"{output_base.name}-*.dat"))
    assert len(outputs) == 1, f"Expected one parity output file, found {len(outputs)}: {outputs}"
    return outputs[0]


def _read_output_rows(dat_path: Path) -> list[list[str]]:
    with dat_path.open(newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.reader(handle) if row]
    assert rows, f"Parity output was empty: {dat_path}"
    assert all(len(row) == 25 for row in rows), f"Unexpected FicTrac row width in {dat_path}"
    return rows


def _canonicalize_rows(rows: list[list[str]]) -> str:
    canonical_rows = []
    for row in rows:
        canonical_rows.append(",".join(row[index].strip() for index in PARITY_KEPT_ZERO_BASED_COLUMNS))
    return "\n".join(canonical_rows) + "\n"


def test_sample_video_parity_contract_ignores_timestamp_columns() -> None:
    base = [str(index) for index in range(25)]
    modified = list(base)
    modified[21] = "1000.0"
    modified[23] = "33.3"
    modified[24] = "45678.0"

    assert _canonicalize_rows([base]) == _canonicalize_rows([modified])


def test_sample_video_parity_matches_native_baseline(tmp_path: Path) -> None:
    _require_sample_parity_assets()

    dat_path = _run_sample_parity(tmp_path)
    rows = _read_output_rows(dat_path)
    assert len(rows) == PARITY_EXPECTED_ROWS

    signature = hashlib.sha256(_canonicalize_rows(rows).encode("utf-8")).hexdigest()
    assert signature == PARITY_EXPECTED_SHA256