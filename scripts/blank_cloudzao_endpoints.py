#!/usr/bin/env python3
"""Blank or verify string literals in main/cloudzao_endpoints.c (open-source gate)."""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "main" / "cloudzao_endpoints.c"
REL = "main/cloudzao_endpoints.c"
NON_EMPTY = re.compile(r'=\s*"[^"]+"')
ZERO = "0" * 40


def find_hits(text: str, label: str) -> list[str]:
    hits: list[str] = []
    for i, line in enumerate(text.splitlines(), 1):
        if NON_EMPTY.search(line):
            hits.append(f"{label}:{i}: {line.strip()}")
    return hits


def report_hits(hits: list[str]) -> int:
    if not hits:
        return 0
    print(
        "Refuse: cloudzao_endpoints.c has non-empty string literals "
        '(open push requires all = "").',
        file=sys.stderr,
    )
    print("\n".join(hits), file=sys.stderr)
    print("Fix: python scripts/blank_cloudzao_endpoints.py", file=sys.stderr)
    return 1


def check_text(text: str, label: str) -> int:
    return report_hits(find_hits(text, label))


def blank_file() -> int:
    if not PATH.is_file():
        print(f"missing {PATH}", file=sys.stderr)
        return 1
    text = PATH.read_text(encoding="utf-8")
    new, n = re.subn(r'=\s*"[^"]*"', '= ""', text)
    PATH.write_text(new, encoding="utf-8", newline="\n")
    print(f"blanked {n} string literal(s) in {PATH.relative_to(ROOT)}")
    return 0


def git_show(rev_path: str) -> str | None:
    try:
        out = subprocess.check_output(
            ["git", "show", rev_path],
            cwd=ROOT,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        return None
    return out.decode("utf-8", errors="replace")


def check_rev(rev: str) -> int:
    text = git_show(f"{rev}:{REL}")
    if text is None:
        return 0
    return check_text(text, f"{rev[:12]}:{REL}")


def check_push_stdin() -> int:
    """Git pre-push: read <local_ref> <local_sha> <remote_ref> <remote_sha> lines.

    Checks each ref tip being pushed (not full history), so older commits that
    already contain literals do not permanently block every push.
    """
    failed = 0
    for line in sys.stdin:
        parts = line.split()
        if len(parts) < 4:
            continue
        local_ref, local_sha, _remote_ref, _remote_sha = parts[:4]
        if local_sha == ZERO:
            continue  # deleting remote ref
        print(f"check cloudzao endpoints blank at {local_ref} ({local_sha[:12]})")
        failed |= check_rev(local_sha)
    return failed


def check_index_or_worktree() -> int:
    text = git_show(f":{REL}")
    if text is None:
        if not PATH.is_file():
            return 0
        text = PATH.read_text(encoding="utf-8")
        return check_text(text, REL)
    return check_text(text, f"index:{REL}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help=f"fail if {REL} has non-empty string literals (worktree)",
    )
    parser.add_argument(
        "--check-index",
        action="store_true",
        help="fail if staged/index version has non-empty literals (pre-commit)",
    )
    parser.add_argument(
        "--check-push",
        action="store_true",
        help="read git pre-push stdin; fail if any pushed commit has non-empty literals",
    )
    parser.add_argument(
        "--check-rev",
        metavar="REV",
        help=f"fail if {REL} at REV has non-empty literals",
    )
    args = parser.parse_args(argv)

    modes = sum(
        bool(x)
        for x in (args.check, args.check_index, args.check_push, args.check_rev)
    )
    if modes > 1:
        print("use only one check mode", file=sys.stderr)
        return 2
    if args.check:
        if not PATH.is_file():
            print(f"missing {PATH}", file=sys.stderr)
            return 1
        return check_text(PATH.read_text(encoding="utf-8"), REL)
    if args.check_index:
        return check_index_or_worktree()
    if args.check_push:
        return check_push_stdin()
    if args.check_rev:
        return check_rev(args.check_rev)
    return blank_file()


if __name__ == "__main__":
    raise SystemExit(main())
