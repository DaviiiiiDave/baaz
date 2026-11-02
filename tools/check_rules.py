"""Rules from REVIEW.md that the compiler cannot check, enforced on every change.

Usage: python tools/check_rules.py   (exits 1 and lists every violation)
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / "addon" / "src"

# R5: the only files that call the Archicad API (everything else stays testable without it)
API_FILES = {"BaazAddon.cpp", "CommandExecutor.cpp", "NavigatorIndexer.cpp"}
# R17: synthesized input exists only for Magic Wand (MessageHook) and the test-only tool path
INPUT_FILES = {"MessageHook.cpp", "CommandExecutor.cpp"}
# R6, R14b: must never be committed
FORBIDDEN_PATHS = [
    (re.compile(r"\.grc\.rc2$|\.reslist\.txt$|\.res$|\.apx$"), "generated resource or build output (R6)"),
    (re.compile(r"(^|/)AddOnFix\.local\.grc$"), "borrowed local-testing MDID (R14)"),
    (re.compile(r"(^|/)check_mdid\.py$"), "local tool that calls Archicad's internal validation code (R14)"),
]
# Tapir's MDID, borrowed for local testing only (R14)
BORROWED_MDID = re.compile(r"\b(826567491|368333322)\b")

CHECKS = [
    # (files the check applies to, pattern, message)
    (lambda name: name not in API_FILES, re.compile(r"\bACAPI_\w+\s*\("),
     "calls the Archicad API outside BaazAddon/CommandExecutor/NavigatorIndexer (R5)"),
    (lambda name: True, re.compile(
        r"\bACAPI_(Element_(Create|Change|Delete|Edit)\w*|CallUndoableCommand|CallCommand"
        r"|ProjectOperation_(Save|Open|Close|NewProject|Quit)\w*|Attribute_(Create|Modify|Delete)\w*"
        r"|Navigator_(Create|Delete|Change(?!CurrLayerComb))\w*|LibraryManagement_\w+)\s*\("),
     "changes the project; Baaz only navigates (R14)"),
    (lambda name: True, re.compile(
        r"#\s*include\s*[<\"](winhttp|wininet|winsock2?|ws2tcpip|urlmon|iphlpapi)\.h"
        r"|\b(URLDownloadToFile\w*|InternetOpen\w*|WinHttpOpen|ShellExecute\w*|CreateProcess\w*|WinExec|system)\s*\("),
     "network access or launching programs (R13)"),
    (lambda name: name not in INPUT_FILES, re.compile(r"\b(SendInput|keybd_event|mouse_event|SetKeyboardState)\s*\("),
     "synthesizes input outside MessageHook/CommandExecutor (R17)"),
    (lambda name: True, re.compile(r"\b(printf|wprintf|OutputDebugString[AW]?|std::w?cout|std::w?cerr)\b"),
     "debug output in the add-on (R8)"),
    (lambda name: True, re.compile(r"//.*\b(TODO|FIXME|XXX|HACK)\b(?!.*(#\d+|https?://))"),
     "TODO without a linked issue (R8)"),
    (lambda name: True, re.compile(r"^\s*//\s*[\w:<>.\->*&\[\]]+(\s*\(.*\)|\s*=.*|\s+\w+\s*=.*);\s*$"),
     "commented-out code (R8)"),
]


def tracked_files() -> list[str]:
    """Files git would commit: tracked plus untracked-but-not-ignored."""
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT, capture_output=True, text=True, check=True,
    )
    return [line for line in result.stdout.splitlines() if line and (ROOT / line).exists()]


def main() -> int:
    files = tracked_files()
    problems: list[str] = []

    for path in files:
        for pattern, message in FORBIDDEN_PATHS:
            if pattern.search(path):
                problems.append(f"{path}: {message}")

    for path in files:
        if path.startswith(("addon/", "tests/")) and BORROWED_MDID.search((ROOT / path).read_text(encoding="utf-8", errors="replace")):
            problems.append(f"{path}: contains a borrowed MDID (R14)")

    for source in sorted(SOURCES.glob("*.cpp")):
        lines = source.read_text(encoding="utf-8").splitlines()
        for number, line in enumerate(lines, start=1):
            for applies, pattern, message in CHECKS:
                if applies(source.name) and pattern.search(line):
                    problems.append(f"{source.relative_to(ROOT).as_posix()}:{number}: {message}")

    for problem in problems:
        print(problem)
    if problems:
        print(f"\n{len(problems)} rule violation(s). See REVIEW.md.")
        return 1
    print("All REVIEW.md checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
