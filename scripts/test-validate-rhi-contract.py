#!/usr/bin/env python3
"""Behavioral tests for `validate-rhi-contract.py`.

All mutated inputs stay in memory; no tracked document is edited. These tests
cover the structural contract of the seven owned documents (sections, fields,
table continuity, Markdown visibility of tables and headings). They do not check
semantic correctness, technical truth, permissions or execution.

Usage:
    py -3 -I scripts/test-validate-rhi-contract.py [VALIDATOR] [SPEC_DIR]
"""

from pathlib import Path
import importlib.util
import sys
import unittest


HERE = Path(__file__).resolve().parent
DEFAULT_VALIDATOR = HERE / "validate-rhi-contract.py"
DEFAULT_SPEC_DIR = HERE.parent / "specs" / "004-quality-renderer"

sys.dont_write_bytecode = True  # loading the validator must not leave __pycache__

validator_path = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else DEFAULT_VALIDATOR
spec_dir = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else DEFAULT_SPEC_DIR

module_spec = importlib.util.spec_from_file_location("validator_under_test", validator_path)
validator = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(validator)


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise AssertionError(f"fixture expected exactly one occurrence of {old!r}")
    return text.replace(old, new, 1)


def extract_table(text, starts):
    lines = text.splitlines(keepends=True)
    index = next(i for i, line in enumerate(lines) if line.startswith(starts))
    end = index
    while end < len(lines) and lines[end].startswith(starts[0]):
        end += 1
    return "".join(lines[index:end])


class ValidatorTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.docs = {name: (spec_dir / name).read_text(encoding="utf-8") for name in validator.FILES}

    def changed(self, name, old, new):
        docs = dict(self.docs)
        docs[name] = replace_once(docs[name], old, new)
        return docs


class Snapshot(ValidatorTestCase):
    def test_complete_snapshot_passes(self):
        self.assertEqual([], validator.validate(self.docs))


class TableContinuity(ValidatorTestCase):
    def test_detached_matrix_row_is_missing(self):
        row_prefix = "| `native-bindless` | Native extension |"
        docs = self.changed("contracts/rhi-foundation.md", row_prefix, "\n" + row_prefix)
        self.assertIn("missing matrix row: native-bindless", validator.validate(docs))

    def test_detached_result_example_is_missing(self):
        row_prefix = "| `<check, adapter or diagnostic>` |"
        docs = self.changed("evidence-template.md", row_prefix, "\n" + row_prefix)
        self.assertIn("missing or incomplete per-check results example row", validator.validate(docs))

    def test_results_separator_garbage_is_invalid(self):
        docs = self.changed("evidence-template.md", "|---|---|---|---|---|",
                            "|oops|oops|oops|oops|oops|")
        self.assertIn("invalid per-check results table separator", validator.validate(docs))

    def test_results_separator_must_be_adjacent(self):
        docs = self.changed("evidence-template.md", "|---|---|---|---|---|",
                            "\n|---|---|---|---|---|")
        self.assertIn("invalid per-check results table separator", validator.validate(docs))

    def test_matrix_separator_garbage_is_invalid(self):
        docs = self.changed("contracts/rhi-foundation.md", "|---|---|---|---|---|",
                            "|oops|oops|oops|oops|oops|")
        self.assertIn("invalid matrix separator", validator.validate(docs))

    def test_matrix_separator_must_be_adjacent(self):
        docs = self.changed("contracts/rhi-foundation.md", "|---|---|---|---|---|",
                            "\n|---|---|---|---|---|")
        self.assertIn("invalid matrix separator", validator.validate(docs))


class MatrixContent(ValidatorTestCase):
    def test_matrix_row_wrong_tier_is_invalid(self):
        docs = self.changed("contracts/rhi-foundation.md", "| `native-dxr` | Native extension |",
                            "| `native-dxr` | Core |")
        self.assertIn("wrong tier for matrix row: native-dxr", validator.validate(docs))

    def test_matrix_row_with_empty_cell_is_invalid(self):
        docs = self.changed("contracts/rhi-foundation.md",
                            "| `native-bindless` | Native extension |",
                            "| `native-bindless` |  |")
        self.assertIn("matrix row requires five nonempty cells: native-bindless",
                      validator.validate(docs))

    def test_duplicate_matrix_row_is_invalid(self):
        row = next(line for line in self.docs["contracts/rhi-foundation.md"].splitlines()
                   if line.startswith("| `native-dxr` |"))
        docs = self.changed("contracts/rhi-foundation.md", row, row + "\n" + row)
        self.assertIn("duplicate matrix row: native-dxr", validator.validate(docs))


class TemplateContent(ValidatorTestCase):
    def test_missing_result_header_is_invalid(self):
        header = ("| Check / target | Execution | Outcome | Reason or observed result "
                  "| Artifact / log reference |")
        docs = self.changed("evidence-template.md", header, "| Check / target | Outcome |")
        self.assertIn("missing per-check results table header", validator.validate(docs))

    def test_missing_required_template_field_is_invalid(self):
        docs = self.changed("evidence-template.md", "- **Commands and exits**:",
                            "- **Renamed**:")
        self.assertIn("missing or empty template field: Commands and exits",
                      validator.validate(docs))


class MarkdownVisibility(ValidatorTestCase):
    def test_fenced_matrix_does_not_count_as_a_table(self):
        name = "contracts/rhi-foundation.md"
        block = extract_table(self.docs[name], "| Operation | Tier |")
        docs = self.changed(name, block, "```text\n" + block + "```\n")
        self.assertIn("missing or invalid matrix header", validator.validate(docs))

    def test_html_commented_matrix_does_not_count(self):
        name = "contracts/rhi-foundation.md"
        block = extract_table(self.docs[name], "| Operation | Tier |")
        docs = self.changed(name, block, "<!--\n" + block + "-->\n")
        self.assertIn("missing or invalid matrix header", validator.validate(docs))

    def test_indented_code_matrix_does_not_count(self):
        name = "contracts/rhi-foundation.md"
        block = extract_table(self.docs[name], "| Operation | Tier |")
        indented = "".join("    " + line for line in block.splitlines(keepends=True))
        docs = self.changed(name, block, indented)
        self.assertIn("missing or invalid matrix header", validator.validate(docs))

    def test_fenced_results_do_not_count_as_a_table(self):
        name = "evidence-template.md"
        block = extract_table(self.docs[name], "| Check / target | Execution |")
        docs = self.changed(name, block, "~~~text\n" + block + "~~~\n")
        self.assertIn("missing per-check results table header", validator.validate(docs))

    def test_fenced_field_does_not_count(self):
        name = "evidence-template.md"
        line = next(line for line in self.docs[name].splitlines(keepends=True)
                    if line.startswith("- **Commands and exits**:"))
        docs = self.changed(name, line, "```\n" + line + "```\n")
        self.assertIn("missing or empty template field: Commands and exits",
                      validator.validate(docs))

    def test_commented_field_does_not_count(self):
        name = "evidence-template.md"
        line = next(line for line in self.docs[name].splitlines(keepends=True)
                    if line.startswith("- **Commands and exits**:"))
        docs = self.changed(name, line, "<!--\n" + line + "-->\n")
        self.assertIn("missing or empty template field: Commands and exits",
                      validator.validate(docs))

    def test_fenced_contract_heading_does_not_count(self):
        name = "contracts/rhi-foundation.md"
        heading = "## Portable core semantics\n"
        docs = self.changed(name, heading, "```\n" + heading + "```\n")
        self.assertIn("missing contract section: Portable core semantics", validator.validate(docs))

    def test_fenced_decoy_does_not_hide_following_visible_matrix(self):
        name = "contracts/rhi-foundation.md"
        block = extract_table(self.docs[name], "| Operation | Tier |")
        decoy = ("```text\n| Operation | Tier | Capability / limit to check | Explicit selectable "
                 "fallback or unsupported | Evidence required |\n|bad|bad|bad|bad|bad|\n```\n\n")
        docs = self.changed(name, block, decoy + block)
        self.assertEqual([], validator.validate(docs))


class EdgeCells(ValidatorTestCase):
    def test_extra_empty_matrix_header_cell_is_invalid(self):
        name = "contracts/rhi-foundation.md"
        line = next(line for line in self.docs[name].splitlines(keepends=True)
                    if line.startswith("| Operation | Tier |"))
        docs = self.changed(name, line, line.rstrip("\r\n") + "|\n")
        self.assertIn("missing or invalid matrix header", validator.validate(docs))

    def test_extra_empty_matrix_row_cell_is_invalid(self):
        name = "contracts/rhi-foundation.md"
        line = next(line for line in self.docs[name].splitlines(keepends=True)
                    if line.startswith("| `core-device` |"))
        docs = self.changed(name, line, line.rstrip("\r\n") + "|\n")
        self.assertIn("matrix row requires five nonempty cells: core-device",
                      validator.validate(docs))

    def test_extra_empty_results_header_cell_is_invalid(self):
        name = "evidence-template.md"
        line = next(line for line in self.docs[name].splitlines(keepends=True)
                    if line.startswith("| Check / target | Execution |"))
        docs = self.changed(name, line, line.rstrip("\r\n") + "|\n")
        self.assertIn("missing per-check results table header", validator.validate(docs))


class ParserRobustness(ValidatorTestCase):
    def test_escaped_pipe_in_matrix_cell_is_valid(self):
        name = "contracts/rhi-foundation.md"
        docs = self.changed(name, "Actual target selection and failure tests",
                            "Actual target selection \\| failure tests")
        self.assertEqual([], validator.validate(docs))

    def test_trailing_space_after_final_pipe_is_valid(self):
        name = "contracts/rhi-foundation.md"
        line = next(line for line in self.docs[name].splitlines(keepends=True)
                    if line.startswith("| `core-device` |"))
        docs = self.changed(name, line, line.rstrip("\r\n") + " \n")
        self.assertEqual([], validator.validate(docs))

    def test_crlf_documents_validate(self):
        docs = {name: text.replace("\r\n", "\n").replace("\n", "\r\n")
                for name, text in self.docs.items()}
        self.assertEqual([], validator.validate(docs))

    def test_second_matrix_table_is_ambiguous(self):
        name = "contracts/rhi-foundation.md"
        block = extract_table(self.docs[name], "| Operation | Tier |")
        docs = self.changed(name, block, block + block)
        self.assertTrue(any(error.startswith("ambiguous matrix table")
                            for error in validator.validate(docs)))

    def test_unknown_matrix_row_is_rejected(self):
        name = "contracts/rhi-foundation.md"
        row = next(line for line in self.docs[name].splitlines(keepends=True)
                   if line.startswith("| `core-device` |"))
        docs = self.changed(name, row, row + "| `native-secret` | Core | x | y | z |\n")
        self.assertIn("unknown matrix row: native-secret", validator.validate(docs))


class SurfaceAndHeaderGate(ValidatorTestCase):
    def test_declared_surface_is_clean(self):
        blocks = validator.fenced_blocks(
            self.docs["contracts/rhi-foundation.md"], validator.SURFACE_INFO)
        self.assertEqual(1, len(blocks))
        self.assertEqual([], validator.prohibited_errors(blocks[0], validator.SURFACE_INFO))

    def test_prohibited_noun_in_surface_is_invalid(self):
        docs = self.changed("contracts/rhi-foundation.md",
                            "adapter, device, capabilities, limits,",
                            "adapter, device, fenceValue, limits,")
        self.assertIn(
            "prohibited D3D12 vocabulary in portable-core-surface: fence/fenceValue",
            validator.validate(docs))

    def test_missing_surface_block_is_invalid(self):
        docs = self.changed("contracts/rhi-foundation.md",
                            "```portable-core-surface\n", "```text\n")
        self.assertIn("missing fenced portable-core-surface block", validator.validate(docs))

    def test_duplicate_surface_block_is_invalid(self):
        name = "contracts/rhi-foundation.md"
        body = validator.fenced_blocks(self.docs[name], validator.SURFACE_INFO)[0]
        docs = dict(self.docs)
        docs[name] = (docs[name] + "\n```" + validator.SURFACE_INFO + "\n"
                      + body + "```\n")
        self.assertIn("ambiguous portable-core-surface block: expected exactly one",
                      validator.validate(docs))

    def test_absolute_path_in_owned_document_is_invalid(self):
        docs = self.changed("data-model.md", "# Design model: Quality-first rendering",
                            "Q:\\fixture\\checkout\n\n# Design model: Quality-first rendering")
        self.assertIn("machine-specific absolute path in data-model.md", validator.validate(docs))

    def test_header_scan_detects_each_private_noun(self):
        errors = validator.header_scan_errors([
            (Path("src/renderer/rhi/rhi.h"),
             "#include <d3d12.h>\n"
             "enum class ResourceState { RENDER_TARGET };\n"
             "uint64_t fenceValue;\nuint64_t GetCompletedValue();\n"
             "uint64_t luid;\nbool enableGpuBasedValidation;\n"
             "struct ID3D12Device;\nDXGI_FORMAT format;\n"
             "ComPtr<IUnknown> device;\n"),
        ])
        for label in ("D3D12", "ID3D12*", "DXGI", "IUnknown", "ComPtr",
                      "ResourceState", "fence/fenceValue", "RenderTarget",
                      "LUID family", "GPU validation/based"):
            self.assertIn(
                "prohibited D3D12 vocabulary in engine-facing header "
                "src\\renderer\\rhi\\rhi.h: " + label, errors)

    def test_public_test_unit_scan_uses_its_own_kind(self):
        errors = validator.header_scan_errors(
            [(Path("tests/renderer/rhi_clear_test.cpp"), "ID3D12Device* device;")],
            "public test unit")
        self.assertIn(
            "prohibited D3D12 vocabulary in public test unit "
            "tests\\renderer\\rhi_clear_test.cpp: ID3D12*", errors)

    def test_header_scan_accepts_portable_names(self):
        errors = validator.header_scan_errors([
            (Path("src/renderer/rhi/rhi.h"),
             "struct TextureView;\nstruct CompletionToken;\nvoid CopyTexture();\n"),
        ])
        self.assertEqual([], errors)

    def test_identifier_embedded_private_forms_are_prohibited(self):
        prefix = ("prohibited D3D12 vocabulary in engine-facing header "
                  "src\\renderer\\rhi\\rhi.h: ")
        cases = {
            "uint64_t adapterLuid;": "LUID family",
            "uint64_t luid_high;": "LUID family",
            "bool EnableGPUOnlyValidation;": "GPU validation/based",
            "struct ValidationFlags { int x; };": "validation flags/layer",
            "enum class Usage { CopySource };": "CopySource/CopyDest",
            "enum class Usage { RenderTarget };": "RenderTarget",
            "struct Presentation {};": "Present*",
            "void PresentFrame();": "Present*",
            "struct CmdList;": "commandAllocator/List/CmdList",
            "void ResetAllocator();": "Reset*",
            "uint64_t completionSerial;": "completion serial/value",
            "void* nativeHandle;": "native/raw handle",
            '#include "dx12/device.h"': "DX12 path",
            "struct DebugFlags { int x; };": "debug layer/flags",
        }
        for text, label in cases.items():
            errors = validator.header_scan_errors(
                [(Path("src/renderer/rhi/rhi.h"), text)])
            self.assertIn(prefix + label, errors, text)

    def test_portable_names_still_pass(self):
        clean = (
            "struct TextureView;\nstruct CompletionToken;\nvoid CopyTexture();\n"
            "float fluidDensity;\nint presentationTime;\nvoid resetFrameCounter();\n"
            "struct AdapterInfo;\n")
        errors = validator.header_scan_errors(
            [(Path("src/renderer/rhi/rhi.h"), clean)])
        self.assertEqual([], errors)
    def test_missing_sparse_aliasing_row_is_invalid(self):
        name = "contracts/rhi-foundation.md"
        row = next(line for line in self.docs[name].splitlines(keepends=True)
                   if line.startswith("| `native-sparse-aliasing` |"))
        docs = self.changed(name, row, "")
        self.assertIn("missing matrix row: native-sparse-aliasing", validator.validate(docs))

    def test_surface_acquire_row_name_is_required(self):
        name = "contracts/rhi-foundation.md"
        row = next(line for line in self.docs[name].splitlines(keepends=True)
                   if line.startswith("| `core-surface-acquire` |"))
        docs = self.changed(name, row, row.replace("`core-surface-acquire`", "`core-present`"))
        self.assertIn("missing matrix row: core-surface-acquire", validator.validate(docs))
        self.assertIn("unknown matrix row: core-present", validator.validate(docs))


if __name__ == "__main__":
    unittest.main(verbosity=2)
