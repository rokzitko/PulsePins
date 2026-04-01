#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path
import sys


CPP_SUFFIXES = {".cc", ".cpp", ".hh", ".h", ".hpp"}
SKIP_PARTS = {"third_party", "include", ".dSYM"}


def iter_cpp_files(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for path in paths:
        if path.is_dir():
            for child in sorted(path.rglob("*")):
                if not child.is_file() or child.suffix not in CPP_SUFFIXES:
                    continue
                if any(part in SKIP_PARTS for part in child.parts):
                    continue
                files.append(child)
        elif path.is_file() and path.suffix in CPP_SUFFIXES:
            files.append(path)
    return files


def check_file(path: Path) -> list[str]:
    errors: list[str] = []
    data = path.read_bytes()

    if b"\r\n" in data:
        errors.append(f"{path}: line endings must be LF")
    if data and not data.endswith(b"\n"):
        errors.append(f"{path}: missing final newline")

    for nr, raw_line in enumerate(data.splitlines(), 1):
        try:
            line = raw_line.decode("utf-8")
        except UnicodeDecodeError:
            errors.append(f"{path}:{nr}: file must be valid UTF-8")
            continue

        if "\t" in line:
            errors.append(f"{path}:{nr}: tab character found")
        if raw_line.endswith(b" ") or raw_line.endswith(b"\t"):
            errors.append(f"{path}:{nr}: trailing whitespace")

        if not line or not line.startswith(" "):
            continue

        leading = len(line) - len(line.lstrip(" "))
        if leading % 2 != 0:
            errors.append(f"{path}:{nr}: indentation {leading} is not divisible by 2")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Check C++ whitespace policy")
    parser.add_argument("paths", nargs="*", default=["c++"], help="Files or directories to scan")
    args = parser.parse_args()

    paths = [Path(p) for p in args.paths]
    files = iter_cpp_files(paths)
    errors: list[str] = []
    for path in files:
        errors.extend(check_file(path))

    for error in errors:
        print(error)

    if errors:
        print(f"Whitespace check failed: {len(errors)} issue(s) in {len(files)} file(s).")
        return 1

    print(f"Whitespace check passed for {len(files)} file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
