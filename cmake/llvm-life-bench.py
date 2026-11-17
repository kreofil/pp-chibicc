#!/usr/bin/env python3
"""Bounded execution and timing of the existing toroidal PPC Life simulation."""
import argparse
from functools import lru_cache
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
from llvm_bench import affinity, digest, measure, settings, summarize, write_tables

VARIANTS = ('native', 'llvm-O2', 'closed-O2')
WORKLOADS = ('blinker-16', 'glider-16', 'random-32', 'random-64')
STEPS, WARMUP, MAX_ROUNDS = 32, 8, 4096
MASK = (1 << 64) - 1
COMMANDS = []


def run(*args):
    command = list(map(str, args))
    COMMANDS.append(command)
    result = subprocess.run(command, text=True, capture_output=True, timeout=120)
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout


def dimensions(mode):
    height = 1 if mode == 4 else 3 if mode == 5 else 16 if mode < 2 else 32 if mode == 2 else 64
    return (5 if mode == 5 else height), height


def initial(mode, seed):
    width, height = dimensions(mode)
    shift_x, shift_y = seed % width, (seed >> 8) % height
    state, rows = seed, []
    for y in range(height):
        row = []
        for x in range(width):
            py, px = (y-shift_y) % height, (x-shift_x) % width
            if mode == 0:
                live = py == height//2 and width//2-1 <= px <= width//2+1
            elif mode == 1:
                live = (py, px) in ((0, 1), (1, 2), (2, 0), (2, 1), (2, 2))
            else:
                state = (state*1664525+1013904223) & 0xffffffff
                live = state >> 24 < 72
            row.append(int(live))
        rows.append(row)
    return rows


def fingerprint(rows):
    result = 14695981039346656037
    for row in rows:
        for value in row:
            result = ((result ^ value)*1099511628211) & MASK
    return result


@lru_cache(maxsize=None)
def reference(mode, seed, steps):
    board = initial(mode, seed)
    width, height = dimensions(mode)
    phases = []
    for phase in range(8):
        state = [row[:] for row in board]
        index = ((seed >> 8)+phase*17) % (width*height)
        state[index//width][index % width] = phase & 1
        for _ in range(steps):
            # Independent scalar model: count eight neighbours, excluding the
            # centre. The original PPC program counts nine and subtracts self.
            counts = [[0]*width for _ in range(height)]
            for y, row in enumerate(state):
                for x, value in enumerate(row):
                    if value:
                        for dy, dx in ((-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)):
                            counts[(y+dy) % height][(x+dx) % width] += 1
            state = [[int(counts[y][x] == 3 or (state[y][x] and counts[y][x] == 2))
                      for x in range(width)] for y in range(height)]
        phases.append(dict(checksum=fingerprint(state), population=sum(map(sum, state))))
    return dict(width=width, height=height, dataset_fingerprint=fingerprint(board), phases=phases)


def data_seed(config, mode):
    return (config['seed'] ^ (mode*0x9e3779b9)) & 0xffffffff


def execute(directory, variant, mode, rounds, seed, steps=STEPS):
    command = [str(directory/'bin'/(variant+'.exe')), str(mode), str(rounds), str(seed), str(steps)]
    result = subprocess.run(command, text=True, capture_output=True, timeout=120)
    if result.returncode or result.stderr:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    sample = json.loads(result.stdout)
    model = reference(mode, seed, steps)
    phases = model['phases']
    totals = {key: sum(p[key] for p in phases) for key in ('checksum', 'population')}
    checksum = (rounds//8*totals['checksum']+sum(p['checksum'] for p in phases[:rounds % 8])) & MASK
    population = rounds//8*totals['population']+sum(p['population'] for p in phases[:rounds % 8])
    expected = dict(mode=mode, rounds=rounds, seed=seed, steps=steps, width=model['width'], height=model['height'],
                    calls=rounds*steps*model['width']*model['height'], warmup_rounds=WARMUP,
                    checksum=checksum, population_sum=population, warmup_checksum=totals['checksum'] & MASK,
                    warmup_population=totals['population'], dataset_fingerprint=model['dataset_fingerprint'])
    if any(sample.get(key) != value for key, value in expected.items()):
        raise RuntimeError('independent Life reference mismatch: '+str(sample)+'; expected '+str(expected))
    for name in ('wall_seconds', 'cpu_seconds'):
        if not math.isfinite(sample[name]) or sample[name] <= 0:
            raise RuntimeError('invalid timer result: '+str(sample))
    sample.update(variant=variant, workload=WORKLOADS[mode] if mode < 4 else 'edge-'+str(mode))
    return sample


def size_of(path):
    sections = {}
    for line in run('size', '-A', path).splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith('.'):
            sections[parts[0]] = int(parts[1])
    return dict(text_bytes=sum(n for name, n in sections.items() if name == '.text' or name.startswith('.text.')),
                file_bytes=path.stat().st_size, sections=sections)


def check_boundary(text):
    header = re.search(r'^define [^\n]*@"?life_run"?\([^\n]*', text, re.M)
    if not header:
        raise RuntimeError('life_run definition disappeared')
    attributes = header[0]
    group = re.search(r'#(\d+)', attributes)
    if group:
        match = re.search(r'^attributes #'+group[1]+r' = \{([^\n]*)', text, re.M)
        attributes += match[1] if match else ''
    if 'noinline' not in attributes:
        raise RuntimeError('life_run noinline boundary disappeared')
    if len(re.findall(r'\bcall\b[^\n]*@"?life_run"?\(', text)) < 2:
        raise RuntimeError('warmup or timed Life round call disappeared')


def build(args, directory):
    root = args.source_dir.resolve()
    for name in ('source', 'raw', 'native', 'llvm', 'bin', 'data', 'tools'):
        (directory/name).mkdir()
    source = directory/'source'
    original = root/'examples/games/conway-life/tor/conway-life-poly-tor/src'
    provenance = {}
    for name in ('map.c', 'map.h', 'eworld.c', 'eworld.h'):
        shutil.copyfile(original/name, source/name)
        provenance[name] = dict(origin=str((original/name).relative_to(root)), sha256=digest(source/name), modified=False)
    for path in (root/'examples/llvm/life-bench').iterdir():
        if path.suffix in ('.h', '.c'):
            shutil.copyfile(path, source/path.name)
            provenance[path.name] = dict(origin=str(path.relative_to(root)), sha256=digest(path), test_harness=True)
    for name in ('llvm-life-bench.py', 'llvm_bench.py', 'ppc-static-dispatch.py', 'ppc-dispatch-report.py'):
        shutil.copyfile(root/'cmake'/name, directory/'tools'/name)
    versions = {name: dict(path=getattr(args, 'llvm_'+name), version=run(getattr(args, 'llvm_'+name), '--version'))
                for name in ('as', 'opt', 'llc', 'link')}
    majors = [re.search(r'LLVM version (\d+)\.', x['version']) for x in versions.values()]
    if any(m is None for m in majors) or len({m[1] for m in majors}) != 1 or int(majors[0][1]) < 19:
        raise ValueError('matching LLVM as/opt/llc/link versions 19+ are required')
    for name in ('as', 'opt', 'llc', 'link'):
        os.environ['PPCHIBICC_LLVM_'+name.upper()] = getattr(args, 'llvm_'+name)
    os.environ['PPCHIBICC_PYTHON'] = sys.executable
    os.environ.pop('PPCHIBICC_STATIC_DISPATCH', None)
    units = ('timer', 'life', 'map', 'eworld')
    raw = []
    for unit in units:
        ir = directory/'raw'/(unit+'.ll')
        run(args.compiler, '-S', '-emit-llvm', '-I'+str(source), source/(unit+'.c'), '-o', ir)
        if unit == 'life':
            text, count = re.subn(r'^(define [^\n]*@"?life_run"?\([^\n]*) \{$', r'\1 noinline {', ir.read_text(), flags=re.M)
            if count != 1:
                raise RuntimeError('cannot mark the Life round boundary')
            ir.write_text(text)
        run(args.llvm_as, ir, '-o', ir.with_suffix('.bc'))
        run(args.llvm_opt, '-passes=verify', '-disable-output', ir)
        raw.append(ir)
    run(sys.executable, root/'cmake/ppc-dispatch-report.py', *raw,
        '--json', directory/'matrices.json', '--markdown', directory/'matrices.md')
    matrix = json.loads((directory/'matrices.json').read_text())
    families = {'DrawCell', 'isLife', 'CreateCell', 'NewCellState'}
    if {x['name'] for x in matrix['functions']} != families or any(x['status'] != 'excluded' for x in matrix['functions']):
        raise RuntimeError('the Life example must retain its four open families')
    closed = directory/'closed.ll'
    run(args.compiler, '--closed-program', '-S', '-emit-llvm', *raw, '-o', closed)
    manifest = json.loads(Path(str(closed)+'.closed.json').read_text())
    if manifest['selected_families'] or manifest['optimization_applied'] or manifest['audit']['removed_runtime']:
        raise RuntimeError('open Life families were unexpectedly frozen')
    run(args.llvm_opt, '-S', '-mcpu=x86-64', '-passes=default<O2>', '-verify-each', closed, '-o', directory/'closed.O2.ll')
    check_boundary((directory/'closed.O2.ll').read_text())
    sizes = {}
    for variant in VARIANTS:
        print('Building Life '+variant, flush=True)
        executable = directory/'bin'/(variant+'.exe')
        if variant == 'closed-O2':
            run(args.compiler, '--closed-program', '-O2', *raw, '-o', executable)
            actual = json.loads(Path(str(executable)+'.closed.json').read_text())
            if actual['selected_families'] or actual['audit']['removed_runtime']:
                raise RuntimeError('public closed compilation froze an open Life family')
        else:
            objects = []
            for unit, ir in zip(units, raw):
                obj = directory/('native' if variant == 'native' else 'llvm')/(unit+'.o')
                if variant == 'native':
                    run(args.compiler, '--backend=x86-64', '-I'+str(source), '-c', source/(unit+'.c'), '-o', obj)
                else:
                    run(args.compiler, '--backend=llvm', '-O2', '-c', ir, '-o', obj)
                    optimized = obj.with_suffix('.ll')
                    run(args.llvm_opt, '-S', '-mcpu=x86-64', '-passes=default<O2>', '-verify-each', ir, '-o', optimized)
                objects.append(obj)
            run(args.compiler, *objects, '-o', executable)
        sizes[variant] = size_of(executable)
    timer = (directory/'llvm/timer.ll').read_text()
    if len(re.findall(r'\bcall\b[^\n]*@"?life_run"?\(', timer)) < 2:
        raise RuntimeError('per-unit LLVM timer lost the round calls')
    return dict(compiler=str(args.compiler), compiler_sha256=digest(args.compiler),
                llvm_tools=versions, code_sizes=sizes, source_files=provenance,
                tool_sha256={p.name: digest(p) for p in (directory/'tools').iterdir()},
                raw_ir_sha256={p.name: digest(p) for p in raw},
                code_size_tool=run('size', '--version'),
                original_root=str(original), closure_manifest=manifest,
                boundary='life_run is noinline in LLVM; the native backend does not inline',
                timer='compiled by the corresponding pipeline; complete-program comparison', commands=COMMANDS)


def checks(directory, config):
    results = []
    for variant in VARIANTS:
        for mode in range(6):
            for seed, steps in ((0, 0), (0xffffffff, 1), (config['seed'], 7), (data_seed(config, mode), STEPS)):
                for rounds in (1, 9):
                    results.append(execute(directory, variant, mode, rounds, seed, steps))
        for bad in (('-1','1','0','1'), ('6','1','0','1'), ('0','0','0','1'),
                    ('0','4097','0','1'), ('0','1','-1','1'), ('0','1','4294967296','1'),
                    ('0','1','0','257'), ('0','1','0','-1'), ('bad','1','0','1'),
                    ('0','1','0',''), ('0','1','+1','1'), ('0','1',' 1','1'), ('0','1','0','1.0'),
                    ('0','1'), ('0','1','0','1','extra')):
            r = subprocess.run([str(directory/'bin'/(variant+'.exe')), *bad], text=True, capture_output=True, timeout=10)
            if r.returncode != 2 or r.stdout or not r.stderr.startswith('usage:'):
                raise RuntimeError('invalid Life argument was accepted: '+str(bad))
    (directory/'checks.json').write_text(json.dumps(results, indent=2)+'\n')
    return results


def report(directory, info, samples):
    lines = ['# Вычислительное ядро «Жизни»: x86-64, LLVM O2 и общая LLVM-сборка', '',
             'Артефакты: `'+str(directory)+'`.',
             'Все результаты сверены с независимой скалярной Python-моделью.',
             'Cell остаётся открытым; ни одна из четырёх таблиц не замораживается.', '',
             'Измеряемый раунд: сброс двух карт, изменение одной клетки (8 фаз), '
             '32 поколения и контрольная сумма итоговой карты. '
             'Создание процесса, конструкторы, создание исходной карты, восемь прогревов и печать исключены.',
             'Таймер обрабатывается соответствующим pipeline. Сравниваются программы целиком; '
             'разность времени не изолирует стоимость одной диспетчеризации.', '']
    if samples:
        summary = summarize(samples, VARIANTS, WORKLOADS)
        write_tables(directory, samples, summary)
        lines += ['Медиана нс/обновление клетки; MAD в скобках. Affinity: '+str(info['affinity']['effective'])+'.', '',
                  '| Нагрузка | x86-64 | LLVM O2 | Общая O2 | LLVM/общая |', '| --- | ---: | ---: | ---: | ---: |']
        for mode, name in enumerate(WORKLOADS):
            values = [f"{summary[mode, v]['median_ns']:.2f} ({summary[mode, v]['mad_percent']:.1f}%)" for v in VARIANTS]
            ratio = summary[mode, 'llvm-O2']['median_ns']/summary[mode, 'closed-O2']['median_ns']
            lines.append(f"| {name} | {' | '.join(values)} | {ratio:.3f} |")
        noisy = sum(x['mad_percent'] > 10 for x in summary.values())
        waiting = sum(x['cpu_seconds']/x['wall_seconds'] < .8 for x in samples)
        lines += ['', f'Рядов с MAD > 10%: {noisy}; замеров с CPU/wall < 0.8: {waiting}. '
                  'MAD характеризует разброс, а не доверительный интервал.', '']
    else:
        lines += ['Выполнены только проверки корректности; статистика времени не собиралась.', '']
    lines += ['| Режим | .text программы, байт | Файл, байт |', '| --- | ---: | ---: |']
    for variant in VARIANTS:
        size = info['build']['code_sizes'][variant]
        lines.append(f"| {variant} | {size['text_bytes']} | {size['file_bytes']} |")
    lines += ['', 'Сохранены build.json, checks.json, datasets.json и карты в data/, '
              'исходники, исходный/оптимизированный IR, матрицы и closure manifests. '
              'При измерении дополнительно сохраняются samples.json/tsv и summary.tsv. '
              'Результаты относятся к данной машине, размерам поля и входам.', '']
    text = '\n'.join(lines)
    (directory/'report.md').write_text(text)
    (directory/'info.json').write_text(json.dumps(info, indent=2)+'\n')
    (directory.parent/'report.md').write_text(text)
    (directory.parent/'info.json').write_text(json.dumps(dict(info, run=str(directory)), indent=2)+'\n')


def main():
    if sys.argv[1:] == ['--check-config']:
        settings()
        return
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-dir', 'compiler', 'llvm-as', 'llvm-opt', 'llvm-llc', 'llvm-link', 'result-dir'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    for name in ('as', 'opt', 'llc', 'link'):
        value = str(getattr(args, 'llvm_'+name))
        found = shutil.which(value)
        if not found:
            raise ValueError('LLVM tool not found: '+value)
        setattr(args, 'llvm_'+name, found)
    if not shutil.which('size'):
        raise ValueError('GNU size is required')
    config = settings()
    selected = affinity(config['cpu'])
    output = args.result_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix='run-', dir=output))
    info = dict(schema='ppchibicc.life-bench.v1', utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                settings=config, affinity=selected, platform=platform.platform(), python=sys.version,
                build=build(args, directory))
    info['executable_sha256'] = {v: digest(directory/'bin'/(v+'.exe')) for v in VARIANTS}
    info['datasets'] = {}
    for mode, name in enumerate(WORKLOADS):
        seed = data_seed(config, mode)
        rows = initial(mode, seed)
        snapshot = directory/'data'/(name+'.txt')
        snapshot.write_text('\n'.join(''.join('*' if v else '.' for v in row) for row in rows)+'\n')
        info['datasets'][name] = dict(reference(mode, seed, STEPS), seed=seed, steps=STEPS,
                                      snapshot_sha256=digest(snapshot))
    (directory/'datasets.json').write_text(json.dumps(info['datasets'], indent=2)+'\n')
    print('Checking Life states, counts and invalid arguments', flush=True)
    info['checks'] = checks(directory, config)
    (directory/'build.json').write_text(json.dumps(info['build'], indent=2)+'\n')
    for variant, checksum in info['executable_sha256'].items():
        if digest(directory/'bin'/(variant+'.exe')) != checksum:
            raise RuntimeError('executable changed during checks')
    samples = []
    if not args.check_only:
        samples = measure(directory, info, VARIANTS, WORKLOADS,
                          lambda variant, mode, rounds: execute(directory, variant, mode, rounds, data_seed(config, mode)),
                          MAX_ROUNDS)
    for mode, name in enumerate(WORKLOADS):
        if digest(directory/'data'/(name+'.txt')) != info['datasets'][name]['snapshot_sha256']:
            raise RuntimeError('dataset changed during measurement')
    report(directory, info, samples)
    print('Life benchmark passed; report: '+str(output/'report.md'), flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError, subprocess.TimeoutExpired) as error:
        sys.exit('LLVM Life benchmark: '+str(error))
