#!/usr/bin/env python3
"""Behavioral and mutation tests for `validate-webgpu-mapping.py`.

Part 1 runs the validator against mutated copies of the real mapping document and
requires each mutation to be rejected with the expected message. Part 2 disables
one protected behavior of the validator at a time in a worktree-local copy and
requires the validator's own self-test to fail on every mutant, then pass again
on the restored source. Neither part edits a tracked file.

Usage:
    py -3 -I scripts/test-validate-webgpu-mapping.py
    py -3 -I scripts/test-validate-webgpu-mapping.py --out build/webgpu-mapping-tests
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
VALIDATOR = HERE / "validate-webgpu-mapping.py"
HEADER = HERE.parent / "src" / "renderer" / "rhi" / "rhi.h"
DOC = HERE.parent / "specs" / "004-quality-renderer" / "webgpu-mapping.md"
DEFAULT_OUT = HERE.parent / "build" / "webgpu-mapping-tests"

DOC_CASES = [
    (
        "missing-row",
        lambda text: re.sub(r"(?m)^\| `AdapterInfo\.deviceId` \|.*?(\r?\n)", "", text, count=1),
        "declaration not mapped: AdapterInfo.deviceId",
    ),
    (
        "stale-row",
        lambda text: text.replace(
            "| `ToString` | Local string table | mapped | — | No GPU operation |",
            "| `ToString` | Local string table | mapped | — | No GPU operation |\n| `Ghost.field` | none | mapped | C1 | |",
            1,
        ),
        "does not correspond to a header declaration",
    ),
    (
        "gap-without-justification",
        lambda text: re.sub(r"(?m)^(\| `AdapterInfo\.deviceId` \|.*\| mapped-with-gap \| C1 \|).*$", r"\1 |", text, count=1),
        "needs a justification",
    ),
    (
        "bad-status",
        lambda text: text.replace("| mapped-with-gap | C1 | The class is inferred", "| todo | C1 | The class is inferred", 1),
        "is not one of",
    ),
    (
        "empty-operation",
        lambda text: text.replace(
            "| `Device.Readback` | `mapAsync(READ)` on a `MAP_READ` buffer, then copy `getMappedRange()` into caller memory and `unmap()` | mapped | C2 | Observed byte-exact `[1,2,3,5,8,13,21,34,55,89,144,233,0,127,128,255]` |",
            "| `Device.Readback` |  | mapped | C2 | Observed byte-exact `[1,2,3,5,8,13,21,34,55,89,144,233,0,127,128,255]` |",
            1,
        ),
        "empty WebGPU operation",
    ),
    (
        "unknown-check",
        lambda text: text.replace("| mapped | C2 | Observed byte-exact", "| mapped | X9 | Observed byte-exact", 1),
        "is not a known check id",
    ),
    (
        "missing-check-row",
        lambda text: text.replace("| C8 | `destroy()` resolves `device.lost` with reason `destroyed` | `destroyed` |\n", "", 1),
        "no row for required check C8",
    ),
    (
        "broken-separator",
        lambda text: text.replace("|---|---|---|---|---|\n", "", 1),
        "no separator row",
    ),
    (
        "short-row",
        lambda text: text.replace(
            "| `ToString` | Local string table | mapped | — | No GPU operation |",
            "| `ToString` | Local string table | mapped |",
            1,
        ),
        "cells, expected",
    ),
    (
        "no-gaps-section",
        lambda text: text.replace("## Gaps and deltas", "## Differences", 1),
        "no '## Gaps' section",
    ),
    (
        "no-mapping-table",
        lambda text: text.replace(
            "| Declaration | WebGPU operation | Status | Conformance check | Justification |",
            "| Name | Operation | State | Check | Notes |",
            1,
        ),
        "no mapping table",
    ),
    (
        "placeholder-justification",
        lambda text: re.sub(r"(?m)^(\| `AdapterInfo\.dedicatedVideoMemory` \|.*\| mapped-with-gap \| C1 \|).*$", r"\1 TODO |", text, count=1),
        "is a placeholder",
    ),
    (
        "missing-negative-check-row",
        lambda text: text.replace("| N2 | Packed-offset control", "| X2 | Packed-offset control", 1),
        "no row for required check N2",
    ),
    (
        "duplicate-row",
        lambda text: text.replace(
            "| `ToString` | Local string table | mapped | — | No GPU operation |",
            "| `ToString` | Local string table | mapped | — | No GPU operation |\n| `ToString` | Local string table | mapped | — | No GPU operation |",
            1,
        ),
        "duplicate mapping row",
    ),
]

MUTANTS = [
    (
        "missing-detection",
        '    for name in missing:\n        errors.append(f"declaration not mapped: {name}")',
        "    for name in missing:\n        pass",
    ),
    (
        "stale-detection",
        '    for name in stale:\n        errors.append(f"mapping row does not correspond to a header declaration: {name}")',
        "    for name in stale:\n        pass",
    ),
    ("status-detection", "        if status not in ALLOWED_STATUS:", "        if False:"),
    ("justification-detection", '        if status != "mapped" and not justification:', "        if False:"),
    ("check-id-detection", "        if check not in CHECK_IDS and check not in NOT_APPLICABLE:", "        if False:"),
    ("duplicate-detection", "        if name in seen:", "        if False:"),
    ("cell-count-detection", "        if len(cells) != len(header):", "        if False:"),
    ("required-check-detection", "        if check not in check_ids:", "        if False:"),
    ("empty-operation-detection", "        if not operation:", "        if False:"),
    (
        "separator-detection",
        '                errors.append("mapping table has no separator row")\n                return header, []',
        "                return header, []",
    ),
    (
        "placeholder-detection",
        '        elif status != "mapped" and justification.strip().strip("`").lower() in PLACEHOLDER_NOTES:',
        "        elif False:",
    ),
    (
        "scoped-enum-detection",
        r'pattern = re.compile(r"\b(enum\s+class|struct|class)\s+(\w+)\s*(?::\s*[\w:]+)?\s*\{")',
        r'pattern = re.compile(r"\b(enum\s+class|struct|class)\s+(\w+)\s*\{")',
    ),
    ("gaps-detection", '    if "## Gaps" not in joined:', "    if False:"),
    (
        "table-detection",
        "    errors.append(\"no mapping table with columns 'Declaration' and 'WebGPU ...'\")",
        "    pass",
    ),
]


def run_validator(validator, header, doc, *extra):
    result = subprocess.run(
        [sys.executable, "-I", str(validator), "--header", str(header), "--doc", str(doc), *extra],
        cwd=HERE.parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=120,
    )
    return result.returncode, result.stdout.decode("utf-8", errors="replace")


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)

    failures = []
    if not VALIDATOR.is_file() or not HEADER.is_file() or not DOC.is_file():
        print("ERROR: expected validator, header and mapping document next to this script")
        return 2

    code, output = run_validator(VALIDATOR, HEADER, DOC)
    if code != 0:
        failures.append(f"baseline document should validate but exited {code}: {output.strip()}")
    else:
        print(f"baseline: {output.strip()}")

    doc_text = DOC.read_text(encoding="utf-8")
    temp = Path(tempfile.mkdtemp(prefix="webgpu-mapping-"))
    try:
        for name, mutate, needle in DOC_CASES:
            mutated = mutate(doc_text)
            if mutated == doc_text:
                failures.append(f"{name}: mutation point not found in the document")
                continue
            path = temp / f"{name}.md"
            path.write_text(mutated, encoding="utf-8")
            code, output = run_validator(VALIDATOR, HEADER, path)
            if code == 0:
                failures.append(f"{name}: mutated document was accepted")
            elif needle not in output:
                failures.append(f"{name}: rejected without the expected message '{needle}'")
            else:
                print(f"document mutation detected: {name}")
    finally:
        shutil.rmtree(temp, ignore_errors=True)

    source = VALIDATOR.read_text(encoding="utf-8")
    mutant_dir = args.out / "mutants"
    mutant_dir.mkdir(parents=True, exist_ok=True)
    for name, old, new in MUTANTS:
        if source.count(old) != 1:
            failures.append(f"{name}: expected exactly one mutation point, found {source.count(old)}")
            continue
        path = mutant_dir / f"validator-{name}.py"
        path.write_text(source.replace(old, new, 1), encoding="utf-8")
        result = subprocess.run(
            [sys.executable, "-I", str(path), "--self-test"],
            cwd=HERE.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        if result.returncode == 0:
            failures.append(f"{name}: self-test still passed with the behaviour disabled")
        else:
            print(f"validator mutant rejected by self-test: {name}")

    code, output = run_validator(VALIDATOR, HEADER, DOC, "--self-test")
    if code != 0:
        failures.append(f"restored validator self-test failed: {output.strip()}")

    if failures:
        print(f"FAILED: {len(failures)} problem(s)")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print(f"OK: {len(DOC_CASES)} document mutations and {len(MUTANTS)} validator mutants behaved as required")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
