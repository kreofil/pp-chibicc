#!/usr/bin/env python3
"""Public closed-program compilation, CMake rebuilds and installed helper lookup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import unittest


def run(*args, env=None, failure=False):
    result = subprocess.run([str(a) for a in args], env=env or ENV, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
    if bool(result.returncode) != failure:
        raise AssertionError(f'{args}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result


class ClosedCompilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = options.source_dir.resolve()
        cls.out = options.result_dir.resolve()
        cls.out.mkdir(parents=True, exist_ok=True)
        cls.example = cls.root/'examples/llvm/closed-registry'
        cls.sources = [cls.example/(name+'.c') for name in ('main', 'special', 'default')]
        cls.objects = []
        for source in cls.sources:
            obj = cls.out/(source.stem+'.o')
            run(options.compiler, '--closed-program', '-c', source, '-o', obj)
            cls.objects.append(obj)

    def compile(self, stem, inputs=None, extra=(), env=None, compiler=None):
        exe = self.out/(stem+'.exe')
        result = run(compiler or options.compiler, '--closed-program', *(inputs or self.objects),
                     *extra, '-o', exe, env=env)
        self.assertEqual('', result.stdout)
        self.assertEqual('closed registry OK\n', run(exe).stdout)
        report = json.loads(Path(str(exe)+'.closed.json').read_text())
        self.assertEqual(['registry_combine'], report['selected_families'])
        self.assertEqual(11, len(report['audit']['removed_runtime']))
        return exe

    def source(self, name, text):
        path = self.out/(name+'.c')
        path.write_text(text)
        return path

    def test_source_and_object_pipelines(self):
        for mode in ('runtime', 'table', 'switch'):
            self.compile('source-'+mode, self.sources, ['--ppc-dispatch='+mode, '-O2'])
        for level in ('0', '3', 's', 'z'):
            self.compile('object-O'+level, extra=['-O'+level, '--closed-family=registry_combine'])

    def test_compile_only_without_external_tools(self):
        env = dict(ENV, PATH='')
        for name in ('AS', 'OPT', 'LLC', 'LINK'):
            env['PPCHIBICC_LLVM_'+name] = '/missing/llvm-tool'
        env.update(PPCHIBICC_PYTHON='/missing/python', PPCHIBICC_STATIC_DISPATCH='/missing/helper')
        obj = self.out/'no-tools.o'
        run(options.compiler, '--closed-program', '-c', self.sources[0], '-o', obj, env=env)
        self.assertTrue(obj.read_text().startswith('; ppchibicc closed-program object v1\n'))
        self.compile('no-tools-linked', [obj, *self.objects[1:]])
        result = run(options.compiler, *self.objects, '-o', self.out/'wrong.exe', failure=True)
        self.assertIn('requires --closed-program', result.stderr)

    def test_raw_ir_mixed_with_source_and_objects(self):
        ir = self.out/'raw.ll'
        run(options.compiler, '-S', '-emit-llvm', self.sources[1], '-o', ir)
        self.compile('mixed-inputs', [self.sources[0], ir, self.objects[2]])

    def test_combined_ir_and_machine_assembly(self):
        ir = self.out/'frozen.ll'
        run(options.compiler, '--closed-program', '-S', '-emit-llvm', *self.objects, '-o', ir)
        self.assertIn('private unnamed_addr constant [6 x ptr]', ir.read_text())
        report = json.loads(Path(str(ir)+'.closed.json').read_text())
        self.assertEqual(hashlib.sha256(ir.read_bytes()).hexdigest(), report['output_sha256'])
        run(options.llvm_as, ir, '-o', self.out/'frozen.bc')
        result = run(options.compiler, '--closed-program', '-S', '-emit-llvm', *self.objects, '-o', '-')
        self.assertTrue(result.stdout.startswith('; ModuleID'))
        asm = self.out/'frozen.s'
        run(options.compiler, '--closed-program', '-O2', '-S', *self.objects, '-o', asm)
        self.assertIn('main:', asm.read_text())

    def test_program_without_eligible_tables(self):
        pure = self.source('pure', 'int main(void) { return 0; }\n')
        exe = self.out/'pure.exe'
        run(options.compiler, '--closed-program', pure, '-o', exe)
        run(exe)
        self.assertFalse(json.loads(Path(str(exe)+'.closed.json').read_text())['optimization_applied'])
        opened = self.source('opened', '''
typedef struct Open {} enum { a } Open;
int choose<Open *x>(void) = 0;
int choose<Open *x>(void) { return 17; }
int main(void) { Open.a x; return choose<&x>() != 17; }
''')
        run(options.compiler, '--closed-program', opened, '-o', exe)
        run(exe)
        report = json.loads(Path(str(exe)+'.closed.json').read_text())
        self.assertFalse(report['optimization_applied'])
        self.assertEqual('excluded', report['unselected_families'][0]['status'])

    def test_extensions_and_paths(self):
        directory = self.out/"space quote' $(literal)"
        directory.mkdir(exist_ok=True)
        shutil.copyfile(self.example/'api.h', directory/'api.pph')
        inputs = []
        for source in self.sources:
            path = directory/(source.stem+'.ppc')
            path.write_text(source.read_text().replace('"api.h"', '"api.pph"'))
            inputs.append(path)
        exe, report = directory/'program.exe', directory/'audit report.json'
        run(options.compiler, '--closed-program', *inputs, '--closed-report='+str(report), '-o', exe)
        self.assertEqual('closed registry OK\n', run(exe).stdout)
        self.assertTrue(report.exists())

    def test_dependencies_and_invalid_options(self):
        dep, obj = self.out/'main.d', self.out/'deps.o'
        run(options.compiler, '--closed-program', '-c', '-MMD', '-MP', '-MT', 'test-target',
            '-MF', dep, self.sources[0], '-o', obj)
        self.assertIn('api.h', dep.read_text())
        for flag in ('-E', '-M'):
            run(options.compiler, '--closed-program', flag, self.sources[0], env=dict(ENV, PATH=''))
        for flags in (['--backend=x86-64'], ['-shared'], ['-MD'], ['-lunknown'], ['-Wl,-s'],
                      ['--closed-family=absent'], ['--closed-family='], ['--closed-report=']):
            run(options.compiler, '--closed-program', *self.objects, *flags,
                '-o', self.out/'invalid.exe', failure=True)
        run(options.compiler, '--closed-family=registry_combine', *self.objects, failure=True)

    def test_native_objects_and_external_modules_are_rejected(self):
        source = self.source('native', 'int main(void) { return 0; }\n')
        obj = self.out/'native.o'
        run(options.host_cc, '-c', source, '-o', obj)
        result = run(options.compiler, '--closed-program', obj, '-o', self.out/'native.exe', failure=True)
        self.assertIn('requires source, raw .ll', result.stderr)
        source = self.source('external', 'int unknown_application(void); int main(void) { return unknown_application(); }\n')
        run(options.compiler, '--closed-program', source, '-o', self.out/'external.exe', failure=True)
        source = self.source('math', '#include <math.h>\nint main(int argc, char **argv) { return sqrt((double)argc) != 1.0; }\n')
        run(options.compiler, '--closed-program', source, '-lm', '-o', self.out/'math.exe')
        run(self.out/'math.exe')

    def test_reallocarray_runtime(self):
        exe = self.out/'reallocarray.exe'
        run(options.compiler, '--closed-program', '-O2',
            self.root/'test/llvm/valid/reallocarray.c', '-o', exe)
        self.assertEqual('reallocarray OK\n', run(exe).stdout)
        report = json.loads(Path(str(exe)+'.closed.json').read_text())
        self.assertFalse(report['optimization_applied'])
        self.assertIn('reallocarray', report['audit']['external_runtime_symbols'])

    def test_dynamic_stack_storage(self):
        for name, expected in (('vla-storage', 'VLA storage OK\n'),
                               ('ppc-multimethod-vla', 'multimethod VLA OK\n'),
                               ('vla-params', 'VLA parameters OK\n'),
                               ('ppc-vla-params', 'PPC VLA parameters OK\n')):
            for mode in ('runtime', 'table', 'switch'):
                for level in ('0', '2'):
                    exe = self.out/(name+'-'+mode+'-O'+level+'.exe')
                    run(options.compiler, '--closed-program', '-O'+level,
                        '--ppc-dispatch='+mode, self.root/'test'/(name+'.c'), '-o', exe)
                    self.assertEqual(expected, run(exe).stdout)
                    report = json.loads(Path(str(exe)+'.closed.json').read_text())
                    if name == 'ppc-vla-params':
                        self.assertIn('vla_matrix', report['selected_families'])
                        self.assertIn('vla_pair', report['selected_families'])
                        self.assertIn('vla_bound_call', report['selected_families'])
                        self.assertTrue(any(f['name'] == 'vla_open' and f['status'] == 'excluded'
                                            for f in report['unselected_families']))
                    else:
                        self.assertEqual(['vla_sum'] if name == 'ppc-multimethod-vla' else [],
                                         report['selected_families'])

    def test_bool_keywords(self):
        for name, expected in (('bool-keywords', 'bool keywords OK\n'),
                               ('ppc-bool', 'PPC bool OK\n')):
            for mode in ('runtime', 'table', 'switch'):
                for level in ('0', '2'):
                    exe = self.out/(name+'-'+mode+'-O'+level+'.exe')
                    run(options.compiler, '--closed-program', '-O'+level,
                        '--ppc-dispatch='+mode, self.root/'test'/(name+'.c'), '-o', exe)
                    self.assertEqual(expected, run(exe).stdout)
                    report = json.loads(Path(str(exe)+'.closed.json').read_text())
                    self.assertEqual(['bool_pair', 'bool_pick', 'bool_result', 'bool_variadic']
                                     if name == 'ppc-bool' else [], report['selected_families'])

    def test_bitfields(self):
        for name, expected in (('bitfield-values', 'bitfield values OK\n'),
                               ('ppc-bitfield', 'PPC bitfield OK\n')):
            for mode in ('runtime', 'table', 'switch'):
                for level in ('0', '2'):
                    exe = self.out/(name+'-'+mode+'-O'+level+'.exe')
                    run(options.compiler, '--closed-program', '-O'+level,
                        '--ppc-dispatch='+mode, self.root/'test'/(name+'.c'), '-o', exe)
                    self.assertEqual(expected, run(exe).stdout)
                    report = json.loads(Path(str(exe)+'.closed.json').read_text())
                    self.assertEqual(['bit_pair', 'bit_pick', 'bit_scalar', 'bit_variadic']
                                     if name == 'ppc-bitfield' else [], report['selected_families'])

    def test_failed_tools_preserve_outputs(self):
        exe = self.out/'preserve.exe'
        report = Path(str(exe)+'.closed.json')
        for variable in ('PPCHIBICC_PYTHON', 'PPCHIBICC_STATIC_DISPATCH', 'PPCHIBICC_LLVM_OPT', 'PPCHIBICC_LLVM_LLC'):
            exe.write_text('old executable')
            report.write_text('old report')
            run(options.compiler, '--closed-program', *self.objects, '-o', exe,
                env=dict(ENV, **{variable: '/missing/tool'}), failure=True)
            self.assertEqual('old executable', exe.read_text())
            self.assertEqual('old report', report.read_text())
        bad = self.source('bad-link', 'int main(void) { return 0; }\n')
        # All preceding tools have absolute paths; ld cannot be found here.
        run(options.compiler, '--closed-program', bad, '-o', exe, env=dict(ENV, PATH=''), failure=True)
        self.assertEqual('old executable', exe.read_text())
        self.assertEqual('old report', report.read_text())
        self.assertFalse(list(self.out.glob('*.ppchibicc-*')))
        run(options.compiler, '--closed-program', *self.objects, '-o', self.objects[0], failure=True)
        run(options.compiler, '--closed-program', *self.objects, '-o', exe,
            '--closed-report='+str(exe), failure=True)

    def test_cmake_incremental_build_and_clean(self):
        sources = self.out/'cmake sources'
        shutil.copytree(self.example, sources, dirs_exist_ok=True)
        build = self.out/'cmake build'
        run(options.cmake, '-S', self.root/'examples/llvm/closed-build', '-B', build,
            '-DCMAKE_C_COMPILER='+str(options.compiler), '-DCLOSED_REGISTRY_SOURCE_DIR='+str(sources))
        run(options.cmake, '--build', build)
        exe = build/'closed-registry'
        report = Path(str(exe)+'.closed.json')
        self.assertEqual('closed registry OK\n', run(exe).stdout)
        stamp = exe.stat().st_mtime_ns
        run(options.cmake, '--build', build)
        self.assertEqual(stamp, exe.stat().st_mtime_ns)
        time.sleep(1.05)
        with (sources/'api.h').open('a') as stream:
            stream.write('\n/* Rebuild every user of this header. */\n')
        run(options.cmake, '--build', build)
        self.assertGreater(exe.stat().st_mtime_ns, stamp)
        run(Path(options.cmake).with_name('ctest'), '--test-dir', build, '--output-on-failure')
        run(options.cmake, '--build', build, '--target', 'clean')
        self.assertFalse(exe.exists())
        self.assertFalse(report.exists())

    def test_installed_compiler_relocation_and_helper_override(self):
        if not options.install_build:
            self.skipTest('installation checked by the CMake test targets')
        prefix, moved = self.out/'installed', self.out/'relocated install'
        for path in (prefix, moved):
            if path.exists():
                shutil.rmtree(path)
        run(options.cmake, '--install', options.install_build, '--prefix', prefix)
        prefix.rename(moved)
        compiler = moved/'bin/ppchibicc'
        self.compile('installed', compiler=compiler)
        self.assertTrue((moved/'share/ppchibicc/PpchibiccClosedProgram.cmake').exists())
        (moved/'share/ppchibicc').rename(moved/'share/hidden')
        run(compiler, '--closed-program', *self.objects, '-o', self.out/'hidden.exe', failure=True)
        self.compile('override', compiler=compiler,
                     env=dict(ENV, PPCHIBICC_STATIC_DISPATCH=str(self.root/'cmake/ppc-static-dispatch.py')))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-dir', 'compiler', 'result-dir', 'cmake'):
        parser.add_argument('--'+name, type=Path, required=True)
    for name in ('host-cc', 'llvm-as', 'llvm-opt', 'llvm-llc', 'llvm-link'):
        parser.add_argument('--'+name, required=True)
    parser.add_argument('--install-build', type=Path)
    options = parser.parse_args()
    options.compiler = options.compiler.resolve()
    options.cmake = options.cmake.resolve()
    ENV = dict(os.environ, PPCHIBICC_PYTHON=sys.executable, PYTHONDONTWRITEBYTECODE='1', LC_ALL='C')
    ENV.pop('PPCHIBICC_STATIC_DISPATCH', None)
    for name in ('as', 'opt', 'llc', 'link'):
        ENV['PPCHIBICC_LLVM_'+name.upper()] = getattr(options, 'llvm_'+name)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
