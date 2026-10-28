#!/usr/bin/env python3
"""Mutation controls for `validate-rhi-contract.py`.

Each mutant disables one protected behavior of the validator. The unchanged
behavioral test file must fail on every mutant and pass on the restored source.
This checks that the tests still detect the behavior they claim to protect; it
does not check the documents' technical truth.

Usage:
    py -3 -I scripts/test-validate-rhi-contract-mutations.py
    py -3 -I scripts/test-validate-rhi-contract-mutations.py --out build-rhi-contract-mutations
"""

import argparse
import difflib
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
DEFAULT_VALIDATOR = HERE / "validate-rhi-contract.py"
DEFAULT_TESTS = HERE / "test-validate-rhi-contract.py"
DEFAULT_SPEC = HERE.parent / "specs" / "004-quality-renderer"
DEFAULT_OUT = HERE.parent / "build-rhi-contract-mutations"


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"expected one mutation point: {old!r}")
    return text.replace(old, new, 1)


def run_tests(tests_path, validator_path, spec_dir):
    result = subprocess.run(
        [sys.executable, "-I", str(tests_path), str(validator_path), str(spec_dir)],
        cwd=HERE.parent, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120,
    )
    return result.returncode, result.stdout.decode("utf-8", errors="replace")


def build_mutants(source):
    """Return (name, source, expected failing test) triples."""
    hidden = replace_once(
        source,
        'template = visible_markdown(docs.get("evidence-template.md", ""))',
        'template = docs.get("evidence-template.md", "")',
    )
    hidden = replace_once(
        hidden,
        'contract = visible_markdown(docs.get("contracts/rhi-foundation.md", ""))',
        'contract = docs.get("contracts/rhi-foundation.md", "")',
    )
    empty_cells = replace_once(
        source,
        "    return tuple(cells)\n",
        "    return tuple(cell for cell in cells if cell)\n",
    )
    detached = replace_once(
        source,
        '            if not re.match(r"^ {0,3}\\|", row):\n                break\n',
        '            if not re.match(r"^ {0,3}\\|", row):\n                index += 1\n'
        "                continue\n",
    )
    separator = replace_once(
        source,
        "        if not re.fullmatch(separator_pattern(len(header)), separator):\n",
        "        if False:\n",
    )
    indented = source.replace('re.match(r"^ {0,3}\\|", lines[index])',
                              're.match(r"^\\s*\\|", lines[index])')
    if indented == source:
        raise RuntimeError("indented-code mutation point missing")
    escaped = replace_once(
        source,
        '        if char == "\\\\" and index + 1 < len(text) and text[index + 1] == "|":\n'
        '            current.append("|")\n'
        "            index += 2\n"
        "            continue\n",
        "        if False:\n"
        "            index += 2\n"
        "            continue\n",
    )
    crlf = replace_once(
        source,
        '    docs = {name: text.replace("\\r\\n", "\\n") for name, text in docs.items()}\n',
        "    pass\n",
    )
    unknown = replace_once(
        source,
        "        if name not in MATRIX_TIERS:\n"
        '            errors.append("unknown matrix row: " + name)\n',
        "",
    )
    ambiguous = replace_once(
        source,
        "    elif len(matrix_tables) > 1:\n"
        '        errors.append("ambiguous matrix table: expected exactly one")\n'
        "        rows = []\n",
        "    elif False:\n"
        "        rows = []\n",
    )
    surface = replace_once(
        source,
        "        errors.extend(prohibited_errors(surface_blocks[0], SURFACE_INFO))\n",
        "        pass\n",
    )
    return (
        ("hidden-markdown", hidden, "test_fenced_matrix_does_not_count_as_a_table"),
        ("extra-empty-cells", empty_cells, "test_extra_empty_matrix_row_cell_is_invalid"),
        ("detached-rows", detached, "test_detached_matrix_row_is_missing"),
        ("invalid-separators", separator, "test_results_separator_garbage_is_invalid"),
        ("indented-code", indented, "test_indented_code_matrix_does_not_count"),
        ("escaped-pipes", escaped, "test_escaped_pipe_in_matrix_cell_is_valid"),
        ("crlf-normalisation", crlf, "test_crlf_documents_validate"),
        ("unknown-matrix-row", unknown, "test_unknown_matrix_row_is_rejected"),
        ("ambiguous-tables", ambiguous, "test_second_matrix_table_is_ambiguous"),
        ("portable-surface-scan", surface, "test_prohibited_noun_in_surface_is_invalid"),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validator", type=Path, default=DEFAULT_VALIDATOR)
    parser.add_argument("--tests", type=Path, default=DEFAULT_TESTS)
    parser.add_argument("--spec-dir", type=Path, default=DEFAULT_SPEC)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    out = args.out.resolve()
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir()
    source = args.validator.read_text(encoding="utf-8")
    digest = hashlib.sha256(args.validator.read_bytes()).hexdigest()

    for name, mutant, expected in build_mutants(source):
        path = out / (name + ".py")
        path.write_text(mutant, encoding="utf-8", newline="\n")
        diff = "".join(difflib.unified_diff(
            source.splitlines(keepends=True), mutant.splitlines(keepends=True),
            fromfile="tracked", tofile=name))
        (out / (name + ".diff")).write_text(diff, encoding="utf-8", newline="\n")
        code, text = run_tests(args.tests, path, args.spec_dir)
        (out / (name + ".log")).write_text(text, encoding="utf-8", newline="\n")
        if code != 1 or ("FAIL: " + expected) not in text:
            print(text[-3000:], flush=True)
            raise RuntimeError("control did not reject: " + name)
        print(f"{name}: rejected by {expected}; exit={code}", flush=True)

    if hashlib.sha256(args.validator.read_bytes()).hexdigest() != digest:
        raise RuntimeError("tracked validator changed during controls")
    code, text = run_tests(args.tests, args.validator, args.spec_dir)
    (out / "restored.log").write_text(text, encoding="utf-8", newline="\n")
    count = re.search(r"Ran (\d+) tests", text)
    if code != 0 or "OK" not in text or not count:
        print(text[-3000:], flush=True)
        raise RuntimeError("restored validator did not pass the unchanged suite")
    print(f"restored: {count.group(1)} tests, exit=0; tracked validator unchanged", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print("MUTATION CONTROLS FAILED:", exc, file=sys.stderr)
        sys.exit(1)
