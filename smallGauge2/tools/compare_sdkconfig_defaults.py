#!/usr/bin/env python3
"""
Generate a diff between the current sdkconfig and the defaults-derived sdkconfig.

Steps performed:
1. Move the existing sdkconfig aside as a temporary backup.
2. Run `idf.py reconfigure` to regenerate sdkconfig from sdkconfig.defaults.
3. Write the textual diff to sdkconfig.diff in the project root.
4. Restore the original sdkconfig (and clean up the generated one).

If any step fails, the original sdkconfig is restored before the script exits.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Sequence


def resolve_idf_command() -> Sequence[str]:
    """Resolve the command that should be used to invoke idf.py."""
    import shutil

    resolved = shutil.which("idf.py")
    if not resolved:
        raise RuntimeError("idf.py not found on PATH")

    path = Path(resolved)
    if path.suffix.lower() in {".bat", ".cmd", ".exe"}:
        return [str(path)]
    if path.suffix.lower() == ".py":
        # Execute the Python script using the current interpreter.
        return [sys.executable, str(path)]
    # Fallback: try executing directly.
    return [str(path)]


def run_idf_py(project_root: Path) -> None:
    """Invoke `idf.py reconfigure` inside the project root."""
    command = list(resolve_idf_command()) + ["reconfigure"]
    try:
        subprocess.run(
            command,
            cwd=project_root,
            check=True,
        )
    except FileNotFoundError as exc:
        raise RuntimeError("idf.py not found on PATH") from exc
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f"idf.py reconfigure failed with code {exc.returncode}") from exc


def write_diff(original: Path, regenerated: Path, destination: Path) -> None:
    """Compute a unified diff between the original and regenerated sdkconfig files."""
    import difflib

    original_lines = original.read_text(encoding="utf-8").splitlines(keepends=True)
    regenerated_lines = regenerated.read_text(encoding="utf-8").splitlines(keepends=True)

    diff = difflib.unified_diff(
        original_lines,
        regenerated_lines,
        fromfile="sdkconfig (original)",
        tofile="sdkconfig (defaults)",
    )

    diff_text = "".join(diff)
    if not diff_text:
        diff_text = "# sdkconfig matches sdkconfig.defaults exactly\n"

    destination.write_text(diff_text, encoding="utf-8")


def main(args: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Compare current sdkconfig to defaults-generated sdkconfig."
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path.cwd(),
        help="Path to ESP-IDF project (default: current directory).",
    )
    parser.add_argument(
        "--diff-path",
        type=Path,
        default=None,
        help="Optional custom path for the diff output file.",
    )
    parsed = parser.parse_args(args)

    project_root = parsed.project_root.resolve()
    sdkconfig = project_root / "sdkconfig"
    defaults = project_root / "sdkconfig.defaults"

    if not defaults.exists():
        raise SystemExit(f"sdkconfig.defaults not found at {defaults}")

    if not sdkconfig.exists():
        raise SystemExit(f"sdkconfig not found at {sdkconfig}")

    diff_path = parsed.diff_path or (project_root / "sdkconfig.diff")

    temp_backup = project_root / "sdkconfig.__codex_backup"
    if temp_backup.exists():
        raise SystemExit(f"Temporary backup already exists: {temp_backup}")

    # Step 1: move original sdkconfig out of the way.
    shutil.move(str(sdkconfig), str(temp_backup))

    generated_config = sdkconfig  # will be created after reconfigure
    try:
        # Step 2: regenerate sdkconfig from defaults.
        run_idf_py(project_root)

        if not generated_config.exists():
            raise RuntimeError("idf.py reconfigure did not produce sdkconfig")

        # Step 3: write diff to sdkconfig.diff (or custom path).
        write_diff(temp_backup, generated_config, diff_path)
    finally:
        # Remove regenerated sdkconfig (if it exists) and restore original.
        if generated_config.exists():
            generated_config.unlink()
        shutil.move(str(temp_backup), str(sdkconfig))

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except RuntimeError as exc:
        raise SystemExit(str(exc))
