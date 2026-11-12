#!/usr/bin/env python3
"""Compile real PPC modules and check the dispatch-report contract."""
import argparse
import itertools
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest


def run(*args, expected=0):
    process = subprocess.run([str(a) for a in args], text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, timeout=60)
    if process.returncode != expected:
        raise AssertionError(f"command {args} returned {process.returncode}, expected {expected}\n"
                             + process.stdout + process.stderr)
    return process


class DispatchReportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = options.source_dir.resolve()
        cls.out = options.result_dir.resolve()
        cls.out.mkdir(parents=True, exist_ok=True)
        cls.example = cls.root / "examples/llvm/closed-registry"
        cls.tool = cls.root / "cmake/ppc-dispatch-report.py"
        versions = []
        for tool in (options.llvm_as, options.llvm_opt, options.llvm_llc, options.llvm_link):
            version = re.search(r"LLVM version (\d+)\.", run(tool, "--version").stdout)
            if not version:
                raise AssertionError("cannot determine LLVM version: " + str(tool))
            versions.append(int(version[1]))
        if len(set(versions)) != 1 or versions[0] < 19:
            raise AssertionError("LLVM tools must have the same major version, at least 19")
        cls.inputs = [cls.compile_file(cls.example / (unit+".c"), unit)
                      for unit in ("main", "special", "default")]
        for path in cls.inputs:
            run(options.llvm_as, path, "-o", path.with_suffix(".bc"))
            run(options.llvm_opt, "-passes=verify", "-disable-output", path.with_suffix(".bc"))
            run(options.llvm_llc, "-O=2", "-filetype=obj", "-relocation-model=pic",
                path.with_suffix(".bc"), "-o", path.with_suffix(".o"))
        cls.report(cls.inputs, stem="report", require=True)
        cls.linked = cls.link(cls.inputs, "linked")

    @classmethod
    def compile_file(cls, path, stem):
        output = cls.out / (stem+".ll")
        run(options.compiler, "--ppc-dispatch=runtime", "-S", "-emit-llvm",
            "-I"+str(cls.example), path, "-o", output)
        return output

    @classmethod
    def compile_text(cls, stem, text):
        path = cls.out / (stem+".c")
        path.write_text(text, encoding="utf-8")
        return cls.compile_file(path, stem)

    @classmethod
    def link(cls, inputs, stem):
        output = cls.out / (stem+".ll")
        run(options.llvm_link, "-S", *inputs, "-o", output)
        return output

    @classmethod
    def report(cls, inputs, stem="check", require=False, expected=0, extra=()):
        json_path = cls.out / (stem+".json")
        md_path = cls.out / (stem+".md")
        run(sys.executable, cls.tool, *inputs, "--json", json_path, "--markdown", md_path,
            *(('--require-resolved',) if require else ()), *extra, expected=expected)
        return json.loads(json_path.read_text(encoding="utf-8"))

    @staticmethod
    def family(report):
        return next(f for f in report["functions"] if f["name"] == "registry_combine")

    def test_matrix_and_default_precedence(self):
        report = self.report(self.inputs, require=True)
        fn = self.family(report)
        self.assertEqual("resolved", fn["status"])
        self.assertEqual([2, 3], [axis["extent"] for axis in fn["axes"]])
        self.assertEqual([[0, 0], [0, 1], [0, 2], [1, 0], [1, 1], [1, 2]],
                         [cell["coordinates"] for cell in fn["cells"]])
        self.assertEqual(["concrete", "default", "default", "default", "default", "concrete"],
                         [cell["kind"] for cell in fn["cells"]])
        self.assertIn("RegistryRight_void", fn["cells"][0]["handlers"][0]["symbol"])
        self.assertIn("RegistryRight_two", fn["cells"][5]["handlers"][0]["symbol"])
        self.assertFalse(report["optimization_applied"])
        self.assertEqual("not-established", report["program_completeness"])

    def test_all_link_orders_and_execution(self):
        expected = self.family(self.report(self.inputs))
        for i, order in enumerate(itertools.permutations(self.inputs)):
            self.assertEqual(expected, self.family(self.report(order)))
            linked = self.link(order, "order-"+str(i))
            self.assertEqual(expected, self.family(self.report([linked])))
            exe = self.out / ("order-"+str(i)+".exe")
            run(options.host_cc, *(p.with_suffix(".o") for p in order), "-o", exe)
            self.assertEqual("closed registry OK\n", run(exe).stdout)

    def test_bitcode_roundtrip_and_optimization(self):
        expected = self.family(self.report(self.inputs))
        for path in (self.linked, self.inputs[2].with_suffix(".bc")):
            output = self.out / (path.stem+"-optimized.ll")
            run(options.llvm_opt, "-S", "-passes=default<O2>", "-verify-each", path, "-o", output)
            fn = self.family(self.report([output]))
            self.assertEqual("resolved", fn["status"])
            self.assertEqual(expected["axes"], fn["axes"])
        output = self.out / "linked-optimized.ll"
        run(options.llvm_llc, "-filetype=obj", "-relocation-model=pic", output, "-o", output.with_suffix(".o"))
        run(options.host_cc, output.with_suffix(".o"), "-o", self.out / "optimized.exe")
        self.assertEqual("closed registry OK\n", run(self.out / "optimized.exe").stdout)

    def test_missing_cells(self):
        fn = self.family(self.report(self.inputs[:2], require=True, expected=1))
        self.assertEqual("incomplete", fn["status"])
        self.assertEqual([1, 2, 3, 4], [c["index"] for c in fn["cells"] if c["state"] == "missing"])

    def test_external_declarations_need_definitions(self):
        decl = self.compile_text("declaration", '#include "api.h"\n'
            'int registry_combine<RegistryLeft.one *a, RegistryRight.two *b>(int x);\n')
        fn = self.family(self.report([decl, self.inputs[2]], require=True, expected=1))
        self.assertEqual("blocked", fn["status"])
        self.assertEqual("unresolved", fn["cells"][5]["state"])
        self.assertEqual("resolved", self.family(self.report([decl]+self.inputs, require=True))["status"])

    def test_opaque_module_blocks_closed_conclusion(self):
        opaque = self.compile_text("opaque", 'struct RegistryLeft; struct RegistryRight;\n'
            'int registry_combine<struct RegistryLeft *a, struct RegistryRight *b>(int x)=0;\n')
        for inputs in ([opaque], self.inputs+[opaque], [self.link(self.inputs+[opaque], "opaque-linked")]):
            fn = self.family(self.report(inputs))
            self.assertEqual("blocked" if len(fn["axes"]) else "excluded", fn["status"])
            self.assertEqual(1, len(fn["excluded_units"]))

    def test_any_open_argument_is_excluded(self):
        mixed = self.compile_text("mixed", 'typedef struct A{} enum { one } const A;\n'
            'typedef struct B{} enum { one } B;\n'
            'int open_left<B*a,A*b>(void){return 1;}\n'
            'int open_right<A*a,B*b>(void){return 2;}\n')
        self.assertEqual(["excluded", "excluded"], [f["status"] for f in self.report([mixed])["functions"]])

    def test_private_overrides_and_llvm_renaming(self):
        text = '#include "api.h"\nstatic int registry_combine<RegistryLeft.one*a,RegistryRight.two*b>(int x){return 900+x;}\n'
        a = self.compile_text("private-a", text)
        b = self.compile_text("private-b", text.replace("900", "800"))
        for inputs in (self.inputs+[a, b], [self.link(self.inputs+[a, b], "private-linked")]):
            fn = self.family(self.report(inputs, require=True, expected=1))
            self.assertEqual("order-dependent", fn["status"])
            self.assertEqual(3, len(fn["cells"][5]["handlers"]))
        linked = self.family(self.report([self.out / "private-linked.ll"]))
        self.assertEqual(3, len({h["symbol"] for h in linked["cells"][5]["handlers"]}))

    def test_private_defaults_do_not_displace_concrete_handlers(self):
        text = '#include "api.h"\nstatic int registry_combine<RegistryLeft*a,RegistryRight*b>(int x){return 900+x;}\n'
        a = self.compile_text("default-private-a", text)
        b = self.compile_text("default-private-b", text.replace("900", "800"))
        fn = self.family(self.report(self.inputs[:2]+[a, b], require=True, expected=1))
        self.assertEqual("order-dependent", fn["status"])
        self.assertEqual(["resolved", "order-dependent", "order-dependent", "order-dependent", "order-dependent", "resolved"],
                         [cell["state"] for cell in fn["cells"]])

    def test_removed_references_or_bodies_are_unresolved(self):
        original = self.inputs[2].read_text()
        without_body, count = re.subn(
            r'^define ([^\n]*@"__ppc_registry_combine_g_g"[^\n]*) \{\n.*?^\}\n',
            r'declare \1\n', original, flags=re.M | re.S)
        self.assertEqual(1, count)
        for stem, text in (("null-reference", "\n".join(
                re.sub(r'ptr @"[^"]+"', 'ptr null', line) if ', !"handler",' in line else line
                for line in original.splitlines())),
                ("no-body", without_body)):
            # A valid module with stale definition metadata must not count
            # a null reference or a declaration as evidence of an IR body.
            path = self.out / (stem+".ll")
            path.write_text(text)
            run(options.llvm_as, path, "-o", path.with_suffix(".bc"))
            fn = self.family(self.report([path], require=True, expected=1))
            self.assertEqual("blocked", fn["status"])
            self.assertTrue(all(cell["state"] == "unresolved" for cell in fn["cells"]))

    def test_inconsistent_closed_types_and_abi(self):
        swapped = self.compile_text("swapped", 'typedef struct RegistryLeft{} enum {one}const RegistryLeft;\n'
            'typedef struct RegistryRight{} enum {two,one}const RegistryRight;\n'
            'int registry_combine<RegistryLeft*a,RegistryRight*b>(int x)=0;\n')
        # A separate source deliberately omits the original int declaration.
        abi = self.compile_text("abi-mismatch", 'typedef struct RegistryLeft{} enum {one}const RegistryLeft;\n'
            'typedef struct RegistryRight{} enum {one,two}const RegistryRight;\n'
            'double registry_combine<RegistryLeft*a,RegistryRight*b>(int x)=0;\n')
        for module in (swapped, abi):
            self.assertEqual("inconsistent", self.family(self.report(self.inputs+[module], require=True, expected=1))["status"])

    def test_duplicate_external_definitions(self):
        duplicate = self.compile_text("duplicate", '#include "api.h"\n'
            'int registry_combine<RegistryLeft*a,RegistryRight*b>(int x){return x;}\n')
        fn = self.family(self.report(self.inputs+[duplicate], require=True, expected=1))
        self.assertEqual("blocked", fn["status"])
        self.assertIn("multiple definitions", " ".join(fn["issues"]))

    def test_report_limit_is_separate_from_local_switch_limit(self):
        fn = self.family(self.report(self.inputs, require=True, expected=1, extra=("--max-cells", "5")))
        self.assertEqual(("limited", 6, []), (fn["status"], fn["cell_count"], fn["cells"]))
        local = self.compile_file(self.root / "test/ppc-closed-multimethod.c", "all-forms")
        families = {f["name"]: f for f in self.report([local])["functions"]}
        self.assertEqual((24, "resolved"), (families["multi_large_grid"]["cell_count"], families["multi_large_grid"]["status"]))
        self.assertEqual({"case", "enum", "nullptr", "union"},
                         {a["form"] for f in families.values() for a in f["axes"]})

    def test_ordinary_c_and_escaped_source_names(self):
        plain = self.compile_text("plain", "int helper(void){return 1;}\n")
        self.assertEqual([], self.report([plain])["functions"])
        escaped = self.compile_text('unit "ёж"', '#include "api.h"\n')
        run(options.llvm_as, escaped, "-o", escaped.with_suffix(".bc"))
        roundtrip = self.out / "escaped-roundtrip.ll"
        run(options.llvm_opt, "-S", "-passes=verify", escaped.with_suffix(".bc"), "-o", roundtrip)
        report = self.report([roundtrip])
        self.assertIn('unit "ёж".c', report["units"][0]["source"])

    def test_invalid_inputs_preserve_reports(self):
        output = self.out / "preserved.json"
        output.write_text("preserved\n", encoding="utf-8")
        invalid = self.out / "invalid.ll"
        variants = ["source_filename = \"foreign.c\"\n",
                    self.inputs[0].read_text().replace("ppchibicc.dispatch.v1", "ppchibicc.dispatch.v999")]
        for text in variants:
            invalid.write_text(text)
            run(sys.executable, self.tool, invalid, "--json", output, expected=2)
            self.assertEqual("preserved\n", output.read_text())
        run(sys.executable, self.tool, self.inputs[0], self.inputs[0], expected=2)
        run(sys.executable, self.tool, self.inputs[0], "--json", self.inputs[0], expected=2)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-dir", "compiler", "host-cc", "llvm-as", "llvm-opt", "llvm-llc", "llvm-link", "result-dir"):
        parser.add_argument("--"+name, required=True, type=Path)
    options = parser.parse_args()
    # Bare executable names must retain PATH lookup instead of becoming ./cc.
    result = unittest.main(argv=[sys.argv[0]], verbosity=2, exit=False).result
    if result.wasSuccessful():
        print("PPC dispatch report passed; report: " + str(options.result_dir / "report.md"))
    sys.exit(0 if result.wasSuccessful() else 1)
