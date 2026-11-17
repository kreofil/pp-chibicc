#!/usr/bin/env python3
"""Check original 4P examples in native, per-unit LLVM and whole-program modes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile

VARIANTS = {
    'native': ['--backend=x86-64'],
    'llvm-O0': ['--backend=llvm', '-O0'],
    'llvm-O2': ['--backend=llvm', '-O2'],
    'closed-O0': ['--closed-program', '-O0'],
    'closed-O2': ['--closed-program', '-O2'],
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, log, env, status=0):
    command = list(map(str, command))
    result = subprocess.run(command, env=env, text=True, capture_output=True, timeout=90)
    log.with_suffix('.command.json').write_text(json.dumps(command, ensure_ascii=False, indent=2)+'\n')
    log.with_suffix('.stdout').write_text(result.stdout)
    log.with_suffix('.stderr').write_text(result.stderr)
    if result.returncode != status:
        raise RuntimeError(f'{shlex.join(command)}: exit {result.returncode}, expected {status}; see {log}.stderr\n'+result.stderr)
    return result


def code_size(executable, log, env):
    result = run(['size', '-A', executable], log, env)
    sections = {}
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0].startswith('.'):
            sections[fields[0]] = int(fields[1])
    return dict(text_bytes=sum(n for name, n in sections.items()
                               if name == '.text' or name.startswith('.text.')),
                file_bytes=executable.stat().st_size, sections=sections)


def report(directory, info):
    lines = ['# Реальные программы 4П: сравнение режимов сборки', '',
             'Исходный репозиторий: '+info['upstream']['repository'],
             'Закреплённый commit: `'+info['upstream']['commit']+'`.',
             'Проверены исходники из: `'+info['source_origin']+'`.',
             'Артефакты этого запуска: `'+str(directory)+'`.', '',
             'Текстовый вывод сверяется с независимыми ожидаемыми значениями; '
             'исторический закрытый spec-param без нулевого обработчика проверяется отдельно.', '',
             '| Пример | Режим | Выполнение | .text, байт | Выбранные семьи | Удалено runtime-функций |',
             '| --- | --- | --- | ---: | --- | ---: |']
    for case in info['cases']:
        for variant, row in case['variants'].items():
            lines.append('| {} | {} | {} | {} | {} | {} |'.format(
                case['name'], variant, row['execution'], row['text_bytes'],
                ', '.join(row.get('selected_families', [])) or '—', row.get('removed_runtime', 0)))
    lines += ['', 'Открытые обобщения остаются открытыми. Для них закрытая сборка '
              'может оптимизировать общий IR, но не замораживает таблицы.', '',
              'spec-param-covered — явно выделенный тестовый адаптер: добавляет '
              'обработчик Figure.void и вызывает его, затем запускает исходный main. '
              'Закреплённый spec-param сохраняется без изменений. '
              'spec-param-current проверяет актуальный открытый пример из ppchibicc, '
              'а spec-param-current-zero вызывает его собственный нулевой обработчик.', '',
              'Время выполнения не измеряется: примеры короткие и в основном печатают текст. '
              'Размер .text относится ко всему исполняемому файлу, включая startup/runtime; '
              'это не измерение быстродействия.', '',
              'report.json содержит SHA256 исходников, компилятора и программ, версии '
              'инструментов, матрицы до преобразования и результаты закрытой сборки. '
              'В каталоге запуска сохраняются исходники, IR, команды, stdout/stderr и манифесты.', '']
    text = '\n'.join(lines)
    (directory/'report.md').write_text(text)
    (directory/'report.json').write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n')
    (directory.parent/'report.md').write_text(text)
    (directory.parent/'report.json').write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-dir', 'compiler', 'result-dir'):
        parser.add_argument('--'+name, required=True, type=Path)
    for name in ('llvm-as', 'llvm-opt', 'llvm-llc', 'llvm-link'):
        parser.add_argument('--'+name, required=True)
    parser.add_argument('--examples-dir', type=Path, help='optional full 4P-examples checkout; default: pinned bundled sources')
    args = parser.parse_args()
    root = args.source_dir.resolve()
    compiler = args.compiler.resolve()
    fixture = root/'examples/llvm/real-programs'
    output = args.result_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix='run-', dir=output))
    # Expected aborts must not leave large core dumps in the source tree.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ, LC_ALL='C', PYTHONDONTWRITEBYTECODE='1', PPCHIBICC_PYTHON=sys.executable)
    env.pop('PPCHIBICC_STATIC_DISPATCH', None)
    for name in ('as', 'opt', 'llc', 'link'):
        env['PPCHIBICC_LLVM_'+name.upper()] = getattr(args, 'llvm_'+name)
    versions = {}
    for name in ('as', 'opt', 'llc', 'link'):
        tool = getattr(args, 'llvm_'+name)
        versions[name] = dict(path=tool, version=run([tool, '--version'], directory/('version-'+name), env).stdout)
    majors = [re.search(r'LLVM version (\d+)\.', v['version']) for v in versions.values()]
    if any(m is None for m in majors) or len({m[1] for m in majors}) != 1 or int(majors[0][1]) < 19:
        raise ValueError('LLVM as/opt/llc/link must have matching major versions, LLVM 19+')
    if not shutil.which('size'):
        raise ValueError('GNU size is required for code-size measurements')
    upstream = json.loads((fixture/'upstream.json').read_text())
    origin = args.examples_dir.resolve() if args.examples_dir else fixture/'upstream'
    source = directory/'source'
    source.mkdir()
    inputs = []
    for entry in upstream['files']:
        rel = entry['path']
        original = origin/rel
        checksum = digest(original)
        if not args.examples_dir and checksum != entry['sha256']:
            raise ValueError('bundled upstream source differs from its manifest: '+rel)
        target = source/rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, target)
        inputs.append(dict(path=rel, sha256=checksum, matches_pinned=checksum == entry['sha256']))
    shutil.copyfile(fixture/'spec-param-covered.c', source/'spec-param-covered.c')
    inputs.append(dict(path='spec-param-covered.c', sha256=digest(source/'spec-param-covered.c'), test_adapter=True))
    current = root/'examples/simplest/spec-param/main.c'
    shutil.copyfile(current, source/'spec-param-current-main.c')
    shutil.copyfile(fixture/'spec-param-current-zero.c', source/'spec-param-current-zero.c')
    inputs += [dict(path='spec-param-current-main.c', source_origin=str(current), sha256=digest(current)),
               dict(path='spec-param-current-zero.c', test_adapter=True,
                    sha256=digest(source/'spec-param-current-zero.c'))]
    actual_commit = None
    if args.examples_dir and (origin/'.git').exists():
        actual_commit = run(['git', '-C', origin, 'rev-parse', 'HEAD'], directory/'upstream-head', env).stdout.strip()
    info = dict(schema='ppchibicc.real-programs.v1', upstream=upstream, source_origin=str(origin),
                actual_upstream_commit=actual_commit, inputs=inputs, llvm_tools=versions,
                compiler=dict(path=str(compiler), sha256=digest(compiler)),
                code_size_tool=run(['size', '--version'], directory/'size-version', env).stdout,
                cases=[], timing_measured=False)
    base = source/'ppc-new-syntax'
    vector = base/'containers/vector-list-lib'
    expected_vector = ''.join(f'{i}: {10-i}\n' for i in range(10))+'\nsum = 55\n'
    expected_vector += ''.join(f'{i}: {i+1}\n' for i in range(10))+'\nvalue = 0; size = 0; capacity = 0\n'
    cases = [
        ('spec-param-original', [base/'simplest/spec-param/main.c'], [], None),
        ('spec-param-covered', [source/'spec-param-covered.c'], [], (fixture/'spec-param-covered.expected').read_text()),
        ('spec-param-current', [source/'spec-param-current-main.c'], [],
         ''.join((fixture/'spec-param-covered.expected').read_text().splitlines(True)[1:])),
        ('spec-param-current-zero', [source/'spec-param-current-zero.c'], [],
         'Incorrect Figure usin tag = 0\n'+''.join((fixture/'spec-param-covered.expected').read_text().splitlines(True)[1:])),
        ('visitor-mm', [base/'patterns/shvets-patterns/visitor-mm.ppc'], [], (fixture/'visitor-mm.expected').read_text()),
        ('ppvector-sum-int', [vector/'examples/ppvector-sum-int/ppvector-sum-int.c',
                            vector/'src/ppvector/ppvector.c', vector/'src/ppvector/ppgeneral.c'],
                            ['-I'+str(vector/'src/include')], expected_vector),
    ]
    for name, sources, flags, expected in cases:
        print('Checking '+name, flush=True)
        folder = directory/name
        folder.mkdir()
        raw = []
        for index, unit in enumerate(sources):
            ir = folder/f'unit-{index}.ll'
            run([compiler, *flags, '-S', '-emit-llvm', unit, '-o', ir], folder/f'raw-{index}', env)
            run([args.llvm_as, ir, '-o', ir.with_suffix('.bc')], folder/f'assemble-{index}', env)
            run([args.llvm_opt, '-passes=verify', '-disable-output', ir], folder/f'verify-{index}', env)
            raw.append(ir)
        matrices = folder/'matrices.json'
        run([sys.executable, root/'cmake/ppc-dispatch-report.py', *raw, '--json', matrices,
             '--markdown', folder/'matrices.md'], folder/'matrices', env)
        matrix = json.loads(matrices.read_text())
        # Save the combined IR before O2 so removed support can be inspected.
        frozen = folder/'closed.ll'
        run([compiler, '--closed-program', '-S', '-emit-llvm', *raw, '-o', frozen], folder/'freeze', env)
        closure = json.loads(Path(str(frozen)+'.closed.json').read_text())
        selected = ['Print'] if name == 'spec-param-covered' else []
        removed = 6 if selected else 0
        if closure['selected_families'] != selected or len(closure['audit']['removed_runtime']) != removed:
            raise RuntimeError('unexpected closure result for '+name)
        if selected:
            text = frozen.read_text()
            if 'private unnamed_addr constant [3 x ptr]' not in text:
                raise RuntimeError('closed Print table was not emitted')
            for symbol in closure['audit']['removed_runtime']:
                if symbol in text:
                    raise RuntimeError('removed runtime function is still in IR: '+symbol)
        else:
            status = 'incomplete' if expected is None else 'excluded'
            if not matrix['functions'] or any(f['status'] != status for f in matrix['functions']):
                raise RuntimeError('unexpected raw family status for '+name)
        if name == 'ppvector-sum-int' and 'reallocarray' not in closure['audit']['external_runtime_symbols']:
            raise RuntimeError('the vector fixture no longer exercises reallocarray')
        if expected is None:
            result = subprocess.run([str(compiler), '--closed-program', '--closed-family=Print',
                                     *map(str, raw), '-o', str(folder/'required.exe')],
                                    env=env, text=True, capture_output=True, timeout=90)
            (folder/'required.stderr').write_text(result.stderr)
            (folder/'required.command.json').write_text(json.dumps(result.args, indent=2)+'\n')
            if result.returncode == 0 or 'Print (incomplete)' not in result.stderr:
                raise RuntimeError('explicitly requiring incomplete Print must fail at build time')
        record = dict(name=name, sources=[str(p.relative_to(source)) for p in sources],
                      adaptation=name in ('spec-param-covered', 'spec-param-current-zero'), raw_matrices=matrix,
                      closure=closure, variants={})
        for variant, options in VARIANTS.items():
            exe = folder/(variant+'.exe')
            run([compiler, *options, *flags, *sources, '-o', exe], folder/(variant+'-build'), env)
            result = run([exe], folder/(variant+'-run'), env,
                         status=-signal.SIGABRT if expected is None else 0)
            if expected is None:
                if result.stdout or result.stderr != 'PPC completeness error: missing handler for Print table slot 0\n':
                    raise RuntimeError('wrong startup diagnostic for '+variant)
            elif result.stdout != expected or result.stderr:
                raise RuntimeError(f'{name}/{variant}: output differs from the expected result')
            row = dict(execution='expected abort: slot 0' if expected is None else 'passed',
                       stdout_sha256=hashlib.sha256(result.stdout.encode()).hexdigest(),
                       executable_sha256=digest(exe), **code_size(exe, folder/(variant+'-size'), env))
            if variant.startswith('closed'):
                manifest = json.loads(Path(str(exe)+'.closed.json').read_text())
                if manifest['selected_families'] != selected or len(manifest['audit']['removed_runtime']) != removed:
                    raise RuntimeError('public compiler closure differs from the inspected IR')
                row.update(selected_families=selected, removed_runtime=removed)
            record['variants'][variant] = row
        info['cases'].append(record)
    report(directory, info)
    print('Real 4P programs passed; report: '+str(output/'report.md'))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print('Real 4P programs: '+str(error), file=sys.stderr)
        sys.exit(1)
