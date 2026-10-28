#!/usr/bin/env python3
"""Check the shape, not the technical truth, of the 004 RHI documentation."""

import argparse
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
SPEC = "specs/004-quality-renderer/"
FILES = (
    "contracts/rhi-foundation.md", "data-model.md", "plan.md", "tasks.md",
    "quickstart.md", "traceability.md", "evidence-template.md",
)
FIELDS = {
    "Identity and scope": (
        "Stage and claim", "Source identity", "Data identity and rights",
        "Scene and inputs", "Exclusions and limitations",
    ),
    "Environment and configuration": (
        "Toolchain and configuration", "Adapter and driver",
        "Capability and fallback", "Shader provenance and build",
    ),
    "Procedure and retained artifacts": (
        "Predeclared tolerances", "Commands and exits",
        "Retained logs and artifacts", "Negative controls",
    ),
    "Independent review": (
        "Independent reviewer", "Review scope and commands",
        "Review decision and unresolved items",
    ),
}
CONTRACT_SECTIONS = (
    "Two tiers and the engine adapter", "Portable core semantics",
    "Capability and fallback matrix", "Native interop and provider boundary",
    "Shader source, build and validation contract", "Public boundary",
    "First vertical-slice acceptance", "T013 row-hash and tolerance policy",
    "Foundation growth required for full Q1",
)
MATRIX_TIERS = {
    "core-device": "Core", "core-resources-bindings": "Core",
    "core-submit-readback": "Core", "core-draw-compute": "Core",
    "core-present": "Core", "native-interop": "Native extension",
    "native-provider": "Native extension", "native-dxr": "Native extension",
    "native-multi-queue": "Native extension", "native-bindless": "Native extension",
}
MATRIX_HEADER = (
    "Operation", "Tier", "Capability / limit to check",
    "Explicit selectable fallback or unsupported", "Evidence required",
)


def visible_markdown(text):
    """Exclude top-level fenced code and HTML comments from structural checks."""
    visible = []
    fence = None
    in_comment = False
    for line in text.splitlines(keepends=True):
        newline = "\n" if line.endswith("\n") else ""
        if fence is not None:
            marker, length = fence
            closing = r"^ {0,3}" + re.escape(marker) + r"{" + str(length) + r",}[ \t]*(?:\r?\n)?$"
            if re.match(closing, line):
                fence = None
            visible.append(newline)
            continue
        pieces = []
        position = 0
        while position < len(line):
            if in_comment:
                end = line.find("-->", position)
                if end < 0:
                    break
                position = end + 3
                in_comment = False
            else:
                start = line.find("<!--", position)
                if start < 0:
                    pieces.append(line[position:])
                    break
                pieces.append(line[position:start])
                position = start + 4
                in_comment = True
        content = "".join(pieces)
        if newline and not content.endswith("\n"):
            content += newline
        opening = re.match(r"^ {0,3}(`{3,}|~{3,})([^\r\n]*)", content)
        if opening and not (opening.group(1)[0] == "`" and "`" in opening.group(2)):
            fence = (opening.group(1)[0], len(opening.group(1)))
            visible.append(newline)
        else:
            visible.append(content)
    return "".join(visible)


def section(text, title):
    match = re.search(
        r"^#{2,3} " + re.escape(title) + r"\s*\n(.*?)(?=^#{1,3} |\Z)",
        text, re.MULTILINE | re.DOTALL,
    )
    return match.group(1) if match else None


def table_cells(line):
    """Split a Markdown table row, treating `\\|` as a literal pipe in a cell."""
    text = line.strip()
    cells = []
    current = []
    index = 1 if text.startswith("|") else 0
    trailing_pipe = False
    while index < len(text):
        char = text[index]
        if char == "\\" and index + 1 < len(text) and text[index + 1] == "|":
            current.append("|")
            index += 2
            continue
        if char == "|":
            cells.append("".join(current).strip())
            current = []
            trailing_pipe = True
            index += 1
            continue
        trailing_pipe = False
        current.append(char)
        index += 1
    if not trailing_pipe:
        cells.append("".join(current).strip())
    return tuple(cells)


def separator_pattern(columns):
    return r" {0,3}\|(?:[ \t]*:?-+:?[ \t]*\|){" + str(columns) + r"}[ \t]*"


def is_table_start(lines, index):
    """True when this pipe row is immediately followed by a matching separator."""
    if index >= len(lines) or not re.match(r"^ {0,3}\|", lines[index]):
        return False
    columns = len(table_cells(lines[index]))
    separator = lines[index + 1] if index + 1 < len(lines) else ""
    return bool(re.fullmatch(separator_pattern(columns), separator))


def tables(body, header):
    """Collect every table with this exact header, stopping rows at its end."""
    lines = body.splitlines()
    found = []
    index = 0
    while index < len(lines):
        if not re.match(r"^ {0,3}\|", lines[index]) or table_cells(lines[index]) != header:
            index += 1
            continue
        separator = lines[index + 1] if index + 1 < len(lines) else ""
        if not re.fullmatch(separator_pattern(len(header)), separator):
            found.append((False, []))
            index += 1
            continue
        rows = []
        index += 2
        while index < len(lines):
            row = lines[index]
            if not re.match(r"^ {0,3}\|", row):
                break
            cells = table_cells(row)
            if cells == header or is_table_start(lines, index):
                break
            rows.append(cells)
            index += 1
        found.append((True, rows))
    return found


def validate(docs):
    docs = {name: text.replace("\r\n", "\n") for name, text in docs.items()}
    errors = []
    for name in FILES:
        if not docs.get(name, "").strip():
            errors.append("missing or empty file: " + name)
    template = visible_markdown(docs.get("evidence-template.md", ""))
    for title, labels in FIELDS.items():
        body = section(template, title)
        if body is None:
            errors.append("missing template section: " + title)
            continue
        for label in labels:
            pattern = r"^- \*\*" + re.escape(label) + r"\*\*:[ \t]*\S[^\r\n]*$"
            if not re.search(pattern, body, re.MULTILINE):
                errors.append("missing or empty template field: " + label)
    results = section(template, "Per-check results") or ""
    for token in ("RUN", "SKIPPED", "NOT RUN", "Outcome", "Artifact / log reference"):
        if token not in results:
            errors.append("missing per-check result field: " + token)
    result_header = (
        "Check / target", "Execution", "Outcome", "Reason or observed result",
        "Artifact / log reference",
    )
    result_tables = tables(results, result_header)
    if not result_tables:
        errors.append("missing per-check results table header")
        rows = []
    elif len(result_tables) > 1:
        errors.append("ambiguous per-check results table: expected exactly one")
        rows = []
    elif not result_tables[0][0]:
        errors.append("invalid per-check results table separator")
        rows = []
    else:
        rows = result_tables[0][1]
    result_rows = [row for row in rows if row and row[0] == "`<check, adapter or diagnostic>`"]
    result_cells = result_rows[0] if result_rows else []
    if len(result_cells) != 5 or any(not cell for cell in result_cells):
        errors.append("missing or incomplete per-check results example row")

    contract = visible_markdown(docs.get("contracts/rhi-foundation.md", ""))
    for title in CONTRACT_SECTIONS:
        if section(contract, title) is None:
            errors.append("missing contract section: " + title)
    matrix = section(contract, "Capability and fallback matrix") or ""
    matrix_tables = tables(matrix, MATRIX_HEADER)
    if not matrix_tables:
        errors.append("missing or invalid matrix header")
        rows = []
    elif len(matrix_tables) > 1:
        errors.append("ambiguous matrix table: expected exactly one")
        rows = []
    elif not matrix_tables[0][0]:
        errors.append("invalid matrix separator")
        rows = []
    else:
        rows = matrix_tables[0][1]
    found = {}
    for row in rows:
        if not row or not re.fullmatch(r"`[a-z0-9-]+`", row[0]):
            errors.append("invalid matrix row")
            continue
        name = row[0][1:-1]
        if name in found:
            errors.append("duplicate matrix row: " + name)
        if name not in MATRIX_TIERS:
            errors.append("unknown matrix row: " + name)
        found[name] = row
        if len(row) != 5 or any(not cell for cell in row):
            errors.append("matrix row requires five nonempty cells: " + name)
    for name, tier in MATRIX_TIERS.items():
        if name not in found:
            errors.append("missing matrix row: " + name)
        elif len(found[name]) > 1 and found[name][1] != tier:
            errors.append("wrong tier for matrix row: " + name)
    return errors


def self_test(docs):
    cases = []
    mutant = dict(docs)
    mutant.pop("data-model.md", None)
    cases.append(("missing file", mutant, "missing or empty file: data-model.md"))
    mutant = dict(docs)
    mutant["evidence-template.md"] = mutant["evidence-template.md"].replace(
        "- **Commands and exits**:", "- **Removed commands**:", 1)
    cases.append(("missing field", mutant, "missing or empty template field: Commands and exits"))
    mutant = dict(docs)
    mutant["evidence-template.md"] = re.sub(
        r"(?m)^- \*\*Commands and exits\*\*:.*$", "- **Commands and exits**:",
        mutant["evidence-template.md"], count=1)
    cases.append(("empty field", mutant, "missing or empty template field: Commands and exits"))
    mutant = dict(docs)
    mutant["evidence-template.md"] = mutant["evidence-template.md"].replace(
        "| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |",
        "| Check / target | Outcome |", 1)
    cases.append(("missing results columns", mutant, "missing per-check results table header"))
    mutant = dict(docs)
    mutant["contracts/rhi-foundation.md"] = mutant["contracts/rhi-foundation.md"].replace(
        "## Portable core semantics", "## Removed semantics", 1)
    cases.append(("missing contract section", mutant, "missing contract section: Portable core semantics"))
    mutant = dict(docs)
    mutant["contracts/rhi-foundation.md"] = mutant["contracts/rhi-foundation.md"].replace(
        "| `native-dxr` | Native extension |", "| `native-dxr` | Core |", 1)
    cases.append(("wrong tier", mutant, "wrong tier for matrix row: native-dxr"))
    mutant = dict(docs)
    mutant["contracts/rhi-foundation.md"] = mutant["contracts/rhi-foundation.md"].replace(
        "| `native-bindless` | Native extension |", "| `native-bindless` |  |", 1)
    cases.append(("empty cell", mutant, "matrix row requires five nonempty cells: native-bindless"))
    for label, replacement in (
        ("separator garbage", "|oops|oops|oops|oops|oops|"),
        ("separator missing cell", "|---|---|---|---|"),
        ("separator extra cell", "|---|---|---|---|---|---|"),
        ("separator empty cell", "|---|---||---|---|"),
        ("separator invalid colon", "|::---|---|---|---|---|"),
        ("separator not adjacent", "\n|---|---|---|---|---|"),
    ):
        mutant = dict(docs)
        mutant["contracts/rhi-foundation.md"] = mutant["contracts/rhi-foundation.md"].replace(
            "|---|---|---|---|---|", replacement, 1)
        cases.append((label, mutant, "invalid matrix separator"))
    for name, mutant, expected in cases:
        if expected not in validate(mutant):
            print("FAIL self-test:", name, "was not detected")
            return 1
        print("PASS self-test:", name, "detected")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="exercise in-memory negative controls")
    args = parser.parse_args()
    docs = {}
    for name in FILES:
        path = ROOT / SPEC / name
        try:
            docs[name] = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            print("ERROR reading", path, ":", exc, file=sys.stderr)
    errors = validate(docs)
    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1
    print("PASS: seven required files, template fields and ten matrix rows have the required structure")
    print("NOTE: structure only; independent semantic review and target execution are NOT RUN by this check")
    return self_test(docs) if args.self_test else 0


if __name__ == "__main__":
    sys.exit(main())
