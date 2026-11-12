#!/usr/bin/env python3
"""Exercise static dispatch with real multi-unit LLVM programs."""
import argparse
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest

sys.dont_write_bytecode = True


def run(*args, expected=0):
    process = subprocess.run([str(arg) for arg in args], text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, timeout=60)
    if process.returncode != expected:
        raise AssertionError(f"{args}: exit {process.returncode}, expected {expected}\n"+process.stdout+process.stderr)
    return process


class StaticDispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = options.source_dir.resolve()
        cls.out = options.result_dir.resolve()
        cls.out.mkdir(parents=True, exist_ok=True)
        cls.tool = cls.root / 'cmake/ppc-static-dispatch.py'
        cls.example = cls.root / 'examples/llvm/closed-registry'
        cls.inputs = {}
        for mode in ('runtime', 'table', 'switch'):
            cls.inputs[mode] = [cls.compile_file(cls.example/(unit+'.c'), mode+'-'+unit, mode)
                                for unit in ('main', 'special', 'default')]
        cls.base = cls.inputs['runtime']
        cls.static, cls.result = cls.freeze(cls.base, 'program')
        cls.baseline = cls.out/'baseline.ll'
        run(options.llvm_link, '-S', *cls.base, '-o', cls.baseline)
        for stem, path in (('baseline', cls.baseline), ('static', cls.static)):
            optimized = cls.out/(stem+'.O2.ll')
            run(options.llvm_opt, '-S', '-passes=default<O2>', '-verify-each', path, '-o', optimized)
            cls.execute(optimized, stem+'-O2', 'closed registry OK\n')
        spec = importlib.util.spec_from_file_location('static_tool', cls.tool)
        cls.reader = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.reader)

    @classmethod
    def compile_file(cls, path, stem, mode='runtime'):
        output = cls.out/(stem+'.ll')
        run(options.compiler, '--ppc-dispatch='+mode, '-S', '-emit-llvm', '-I'+str(cls.example), path, '-o', output)
        return output

    @classmethod
    def compile_text(cls, stem, text):
        source = cls.out/(stem+'.c')
        source.write_text(text)
        return cls.compile_file(source, stem)

    @classmethod
    def freeze(cls, inputs, stem, expected=0, extra=(), closed=True):
        output = cls.out/(stem+'.ll')
        report = cls.out/(stem+'.json')
        run(sys.executable, cls.tool, *inputs, *(('--closed-program',) if closed else ()),
            '--llvm-as', options.llvm_as, '--llvm-opt', options.llvm_opt, '--llvm-link', options.llvm_link,
            '-o', output, '--report', report, *extra, expected=expected)
        return output, json.loads(report.read_text()) if expected == 0 else None

    @classmethod
    def execute(cls, path, stem, expected):
        obj, exe = cls.out/(stem+'.o'), cls.out/(stem+'.exe')
        run(options.llvm_llc, '-O=2', '-filetype=obj', '-relocation-model=pic', path, '-o', obj)
        run(options.host_cc, obj, '-o', exe)
        cls_output = run(exe)
        if cls_output.stdout != expected or cls_output.stderr:
            raise AssertionError('execution mismatch: '+cls_output.stdout+cls_output.stderr)

    def test_immutable_table_and_removed_lifecycle(self):
        result = self.result
        self.assertTrue(result['optimization_applied'])
        self.assertEqual('explicit-input-set', result['program_completeness'])
        self.assertEqual(['registry_combine'], result['selected_families'])
        self.assertEqual(11, len(result['audit']['removed_runtime']))
        text = self.static.read_text()
        self.assertIn('private unnamed_addr constant [6 x ptr]', text)
        self.assertIn('internal constant ptr', text)
        self.assertIn('!ppchibicc.closed_program', text)
        for name in result['audit']['removed_runtime']:
            self.assertNotIn(name, text)
        self.assertEqual(hashlib.sha256(text.encode()).hexdigest(), result['output_sha256'])
        self.assertEqual(3, len(result['inputs']))
        self.execute(self.static, 'static-unoptimized', 'closed registry OK\n')

    def test_known_call_becomes_constant(self):
        before = self.reader.functions((self.out/'baseline.O2.ll').read_text())['registry_known']['body']
        after = self.reader.functions((self.out/'static.O2.ll').read_text())['registry_known']['body']
        self.assertIn('call i32 %', before)
        self.assertIn('ret i32 507', after)
        self.assertNotIn('call ', after)
        lines = ['# Статические таблицы закрытой диспетчеризации', '',
            'Программа из трёх явно перечисленных LLVM-модулей; матрица `2×3`.', '',
            'Удалено 11 функций выделения, заполнения, проверки и освобождения этой таблицы.', '',
            '`registry_known` после O2:', '', '```llvm', after.strip(), '```', '',
            'При runtime-таблице здесь сохраняется косвенный вызов. Статический вариант возвращает 507.', '',
            'Динамические признаки продолжают индексировать неизменяемый массив; это не измерение быстродействия.', '']
        (self.out/'report.md').write_text('\n'.join(lines), encoding='utf-8')

    def test_all_modes_and_module_orders(self):
        for mode, paths in self.inputs.items():
            for index, order in enumerate(itertools.permutations(paths)):
                output, _ = self.freeze(order, mode+'-order-'+str(index))
                self.execute(output, mode+'-order-'+str(index), 'closed registry OK\n')

    def test_multimethods_records_varargs_and_open_neighbors(self):
        for mode in ('runtime', 'table', 'switch'):
            sources = ('test/llvm/valid/ppc-closed-multimethod.c',
                       'test/llvm/support/closed-multimethod-override.c',
                       'test/llvm/support/closed-multimethod-opaque.c')
            inputs = [self.compile_file(self.root/source, 'multi-'+mode+'-'+str(i), mode)
                      for i, source in enumerate(sources)]
            output, report = self.freeze(inputs, 'multi-'+mode)
            self.assertIn('multi_late', report['selected_families'])
            self.assertNotIn('multi_remote', report['selected_families'])
            self.assertNotIn('multi_open_left', report['selected_families'])
            self.assertTrue({4, 6, 12, 24} <= {f['cell_count'] for f in report['functions']})
            for level in (0, 2):
                optimized = self.out/('multi-'+mode+'-O'+str(level)+'.ll')
                run(options.llvm_opt, '-S', '-passes='+('verify' if level == 0 else 'default<O2>'),
                    '-verify-each', output, '-o', optimized)
                self.execute(optimized, 'multi-'+mode+'-O'+str(level), 'closed multimethod OK\n')

    def test_explicit_program_contract_entry_and_limit(self):
        self.freeze(self.base, 'no-contract', expected=2, closed=False)
        self.freeze(self.base[1:], 'no-entry', expected=2)
        self.freeze(self.base, 'too-small', expected=2, extra=('--family', 'registry_combine', '--max-cells', '5'))
        self.freeze(self.base, 'absent', expected=2, extra=('--family', 'absent'))

    def test_incomplete_and_opaque_and_order_dependent(self):
        self.freeze(self.base[:2], 'incomplete', expected=2, extra=('--family', 'registry_combine'))
        opaque = self.compile_text('opaque', 'struct RegistryLeft; struct RegistryRight;\n'
            'int registry_combine<struct RegistryLeft*a,struct RegistryRight*b>(int x)=0;\n')
        override = self.compile_text('private', '#include "api.h"\n'
            'static int registry_combine<RegistryLeft.one*a,RegistryRight.two*b>(int x){return 900+x;}\n')
        for extra in (opaque, override):
            self.freeze(self.base+[extra], 'blocked-'+extra.stem, expected=2,
                        extra=('--family', 'registry_combine'))

    def test_unknown_native_dependency(self):
        extra = self.compile_text('external', 'int external_user_module(void);\n'
            'int invoke_external(void){return external_user_module();}\n')
        self.freeze(self.base+[extra], 'external-blocked', expected=2)
        weak = self.compile_text('weak-data', 'extern int native_state; int get_state(void){return native_state;}\n')
        weak.write_text(weak.read_text().replace('= external global', '= extern_weak global'))
        self.freeze(self.base+[weak], 'weak-data-blocked', expected=2)
        foreign = self.out/'native.o'
        foreign.write_bytes(b'not an LLVM translation unit')
        self.freeze(self.base+[foreign], 'native-blocked', expected=2)

    def test_table_writes_and_address_escapes(self):
        prefix = 'extern void **__ppc_registry_combine_table;\n'
        bodies = ['void touch(void){__ppc_registry_combine_table=0;}',
                  'void touch(void){__ppc_registry_combine_table[0]=0;}',
                  'void free(void*); void touch(void){free(__ppc_registry_combine_table);}',
                  'long touch(void){return (long)__ppc_registry_combine_table;}',
                  'void *alias=&__ppc_registry_combine_table;']
        for i, body in enumerate(bodies):
            extra = self.compile_text('escape-'+str(i), prefix+body+'\n')
            self.freeze(self.base+[extra], 'escape-blocked-'+str(i), expected=2)

    def test_runtime_metadata_and_priorities_are_checked(self):
        original = self.base[1].read_text()
        match = re.search(r'^!(\d+) = .* !"runtime".* !"allocate", ptr (@"[^"]+")', original, re.M)
        self.assertIsNotNone(match)
        ident, token = match.groups()
        root = re.search(r'^!ppchibicc.dispatch = !\{([^\n]*)\}', original, re.M)
        ids = [i for i in re.findall(r'!(\d+)', root[1]) if i != ident]
        variants = [original[:root.start()]+ '!ppchibicc.dispatch = !{'+', '.join('!'+i for i in ids)+'}'+original[root.end():],
                    original.replace('i32 301, ptr ', 'i32 302, ptr ', 1),
                    original+'\ndefine void @unexpected_startup_call() {\nentry:\n  call void '+token+'()\n  ret void\n}\n']
        for i, text in enumerate(variants):
            altered = self.out/('altered-'+str(i)+'.ll')
            altered.write_text(text)
            self.freeze([self.base[0], altered, self.base[2]], 'audit-blocked-'+str(i), expected=2)

    def test_outputs_are_preserved_on_failure(self):
        output = self.out/'preserved.ll'
        report = self.out/'preserved.json'
        output.write_text('preserved IR\n')
        report.write_text('preserved report\n')
        self.freeze(self.base[:2], 'preserved', expected=2, extra=('--family', 'registry_combine'))
        self.assertEqual('preserved IR\n', output.read_text())
        self.assertEqual('preserved report\n', report.read_text())
        self.freeze(self.base, 'preserved', expected=2, extra=('--report', self.out/'missing-directory'/'report.json'))
        self.assertEqual('preserved IR\n', output.read_text())

    def test_linked_optimized_and_legacy_inputs_are_not_frozen(self):
        for path in (self.baseline, self.out/'baseline.O2.ll'):
            self.freeze([path], 'not-raw', expected=2)
        ordinary = self.compile_text('legacy', 'int ordinary(void){return 1;}\n')
        ordinary.write_text(ordinary.read_text().replace('ppchibicc.dispatch.v2', 'ppchibicc.dispatch.v1'))
        run(sys.executable, self.root/'cmake/ppc-dispatch-report.py', ordinary)
        self.freeze(self.base+[ordinary], 'legacy-blocked', expected=2)

    def test_target_and_layout_must_match(self):
        original = self.base[1].read_text()
        for i, text in enumerate((original.replace('x86_64-pc-linux-gnu', 'aarch64-pc-linux-gnu'),
                                 original.replace('i64:64-f80:128', 'i64:32-f80:128'))):
            path = self.out/('wrong-target-'+str(i)+'.ll')
            path.write_text(text)
            self.freeze([self.base[0], path, self.base[2]], 'wrong-target-blocked-'+str(i), expected=2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-dir', 'compiler', 'host-cc', 'llvm-as', 'llvm-opt', 'llvm-llc', 'llvm-link', 'result-dir'):
        parser.add_argument('--'+name, required=True, type=Path)
    options = parser.parse_args()
    result = unittest.main(argv=[sys.argv[0]], verbosity=2, exit=False).result
    if result.wasSuccessful():
        print('Static PPC dispatch passed; report: '+str(options.result_dir/'report.md'))
    sys.exit(0 if result.wasSuccessful() else 1)
