#!/usr/bin/env python3
"""Enforce the sim boundary as a test rather than a review item.

ARCHITECTURE.md section 1 draws one line through this codebase: everything
under src/sim/ is deterministic, and everything else may read that state but
never write it. Every rule below is a desync that *compiles*, passes local
tests, and fails only in a real match between two different machines.

Those are exactly the bugs a human reviewer misses, because catching them means
noticing an absence rather than a mistake. So we grep for them on every CI run.

This is intentionally a lexical check, not a parse. A real check would need a
compiler frontend; this catches the realistic failure -- someone reaches for a
familiar tool without thinking about which side of the line they are on -- at
roughly none of the cost.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Each rule is (name, compiled pattern, why it matters).
#
# The explanations are load-bearing: an agent or a contributor who trips one of
# these needs to understand the failure mode, or the next commit works around
# the check instead of fixing the cause.
RULES: list[tuple[str, re.Pattern[str], str]] = [
    (
        "floating point",
        re.compile(r"\b(float|double)\b|\b\d+\.\d+[fF]?\b|\b\d+[fF]\b(?![\w\"])"),
        "Compilers contract multiply-add, keep different intermediate precision, "
        "and vectorize in different orders. Two machines then disagree. Use Fixed "
        "(src/sim/fixed.h).",
    ),
    (
        "math header",
        re.compile(r"#\s*include\s*<cmath>|#\s*include\s*<math\.h>|\bstd::(sin|cos|sqrt|pow|fabs)\b"),
        "Transcendental functions are not bit-identical across libm "
        "implementations. Use lookup tables in src/sim/math/.",
    ),
    (
        "heap allocation",
        re.compile(r"\bnew\s+\w|\bmalloc\s*\(|\bcalloc\s*\(|\brealloc\s*\(|\bdelete\b"),
        "GameState must stay trivially copyable so rollback save/restore is a "
        "memcpy (ADR 0005). Use fixed-size arrays.",
    ),
    (
        "std container",
        re.compile(
            r"\bstd::(vector|map|set|unordered_map|unordered_set|string|string_view"
            r"|list|deque|shared_ptr|unique_ptr|function|optional|variant)\b"
        ),
        "These allocate, or store pointers, or both -- each of which breaks the "
        "memcpy contract. Unordered ones additionally iterate in an "
        "implementation-defined order, which desyncs directly.",
    ),
    (
        "clock read",
        re.compile(r"#\s*include\s*<chrono>|\bstd::chrono\b|\btime\s*\(|\bclock\s*\(|SDL_GetTicks"),
        "The sim has no dt and no notion of wall time (ADR 0002). Durations are "
        "integer frame counts at 60Hz.",
    ),
    (
        "ambient RNG",
        re.compile(r"\brand\s*\(|\bsrand\s*\(|\bstd::(mt19937|random_device|uniform_\w+)\b"),
        "RNG state must live inside GameState so rollback restores it. The "
        "standard library's distributions are also unspecified across "
        "implementations. Use src/sim/rng.h.",
    ),
    (
        "I/O",
        re.compile(r"\b(printf|fprintf|puts|fopen|ifstream|ofstream|cout|cerr)\b|MW_LOG_"),
        "Rollback re-simulates the same frame up to 8x, so a side effect fires "
        "8x. Observe the sim from outside instead: training mode or "
        "tools/replay_inspector/.",
    ),
    (
        "exceptions or RTTI",
        re.compile(r"\bthrow\b|\btry\b\s*\{|\bdynamic_cast\b|\btypeid\b"),
        "Forbidden by ADR 0001. The sim has no error path (CONVENTIONS.md 3) -- "
        "everything it needs was validated at load time.",
    ),
    (
        "upward dependency",
        re.compile(r"#\s*include\s*[\"<](render|audio|ui|platform)/"),
        "The dependency direction is one-way (ARCHITECTURE.md 1). The sim is "
        "testable without SDL precisely because it includes nothing above it.",
    ),
]

# Lines opting out must say which rule and why, e.g.
#   // sim-boundary-allow: std container -- compile-time only, never in a frame
ALLOW_RE = re.compile(r"//\s*sim-boundary-allow:\s*([^-\n]+?)\s*--\s*(.+)")

# A line that is entirely a comment cannot generate code, and the rule
# explanations in the sim's own headers name the forbidden constructs
# constantly. Checking comments would mean deleting the documentation that
# makes the rules followable.
COMMENT_RE = re.compile(r"^\s*(//|\*|/\*)")


def strip_strings(line: str) -> str:
    """Blank out string and char literals so their contents cannot match."""
    line = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)
    return re.sub(r"'(?:[^'\\]|\\.)*'", "''", line)


def check_file(path: Path) -> list[str]:
    findings: list[str] = []
    in_block_comment = False

    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        stripped = raw.strip()

        if in_block_comment:
            if "*/" in stripped:
                in_block_comment = False
            continue
        if stripped.startswith("/*") and "*/" not in stripped:
            in_block_comment = True
            continue
        if COMMENT_RE.match(stripped):
            continue

        # Drop trailing comments, then string contents.
        code = strip_strings(raw.split("//", 1)[0])
        if not code.strip():
            continue

        allowed = ALLOW_RE.search(raw)
        allowed_rule = allowed.group(1).strip() if allowed else None

        for name, pattern, why in RULES:
            match = pattern.search(code)
            if not match:
                continue
            if allowed_rule == name:
                continue
            findings.append(
                f"{path.as_posix()}:{number}: [{name}] found {match.group(0)!r}\n"
                f"    {why}\n"
                f"    | {stripped}"
            )

    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "sim_dir",
        type=Path,
        nargs="?",
        default=Path(__file__).resolve().parent.parent / "src" / "sim",
        help="directory to check (default: src/sim)",
    )
    args = parser.parse_args()

    if not args.sim_dir.is_dir():
        print(f"error: {args.sim_dir} is not a directory", file=sys.stderr)
        return 2

    sources = sorted(
        p for p in args.sim_dir.rglob("*") if p.suffix in {".h", ".hpp", ".cpp", ".inl"}
    )
    if not sources:
        # An empty check that reports success is worse than no check, because it
        # reports green forever after someone moves the directory.
        print(f"error: no sources found under {args.sim_dir}", file=sys.stderr)
        return 2

    findings: list[str] = []
    for source in sources:
        findings.extend(check_file(source))

    if findings:
        print(f"sim boundary violated -- {len(findings)} finding(s):\n", file=sys.stderr)
        for finding in findings:
            print(finding + "\n", file=sys.stderr)
        print(
            "Each of these is a desync that compiles and passes local tests.\n"
            "See ARCHITECTURE.md section 1. If a use is genuinely safe, annotate the\n"
            'line: // sim-boundary-allow: <rule name> -- <why it cannot desync>',
            file=sys.stderr,
        )
        return 1

    print(f"sim boundary clean: {len(sources)} file(s) checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
