#!/usr/bin/env python3
"""Build, check and measure four LLVM pipelines for closed multimethods."""
import argparse
import csv
from functools import lru_cache
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys
import time

sys.dont_write_bytecode = True
from llvm_bench import affinity, digest, measure, settings, summarize, write_tables

VARIANTS = ('runtime', 'switch', 'static-table', 'static-switch')
KERNELS = ('bench_pair_short', 'bench_pair_work', 'bench_triple_short', 'bench_triple_work')
FAMILIES = {'bench_short2', 'bench_work2', 'bench_short3', 'bench_work3'}
WORKLOADS = tuple(f'{arity}D-{kind}-{distribution}' for arity in (2, 3)
                  for kind in ('short', 'work') for distribution in ('uniform', 'hot90'))
BATCH, WARMUP, MAX_ROUNDS = 4096, 8, 1000000
MASK = (1 << 64) - 1
COMMANDS = []


def run(*args):
    command = [str(arg) for arg in args]
    COMMANDS.append(command)
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f'{command} exited with {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout


def load_sealer(root):
    spec = importlib.util.spec_from_file_location('static_dispatch', root/'cmake/ppc-static-dispatch.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def object_sizes(obj):
    sections = {}
    for line in run('size', '-A', obj).splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0].startswith('.'):
            sections[fields[0]] = int(fields[1])
    symbols = {}
    for line in run('nm', '-S', '--defined-only', obj).splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[2] in ('T', 't'):
            symbols[fields[3]] = int(fields[1], 16)
    return sections, symbols


def text_size(sections):
    return sum(size for section, size in sections.items() if section == '.text' or section.startswith('.text.'))


def partition(text, timer_names, reader):
    definitions = reader.functions(text)
    if not timer_names <= definitions.keys() or not set(KERNELS) <= definitions.keys():
        raise RuntimeError('timer/kernel definitions changed during linking')
    edits = []
    for name, fn in definitions.items():
        start, end = fn['span']
        if name in timer_names:
            edits.append((start, end, ''))
        elif name in KERNELS:
            # Preserve one observable batch boundary in every variant. The
            # handlers inside each kernel remain available for LLVM inlining.
            position = start + len('define ') + len(fn['header'])
            edits.append((position, position, ' noinline'))
    for start, end, replacement in sorted(edits, reverse=True):
        text = text[:start]+replacement+text[end:]
    if set(reader.globals_in(text)) & timer_names:
        raise RuntimeError('core unexpectedly refers to a timer function')
    return text


def build(args, directory):
    root = args.source_dir.resolve()
    example = root/'examples/llvm/static-dispatch-bench'
    versions = {name: run(getattr(args, 'llvm_'+name), '--version') for name in ('as', 'opt', 'llc', 'link')}
    majors = [re.search(r'LLVM version (\d+)\.', version) for version in versions.values()]
    if any(match is None for match in majors) or len({match[1] for match in majors}) != 1 or int(majors[0][1]) < 19:
        raise ValueError('llvm-as, opt, llc and llvm-link must have the same major version, at least 19')
    if not shutil.which('size') or not shutil.which('nm'):
        raise ValueError('GNU size and nm are required for code-size measurements')
    for name in ('source', 'source/tools', 'input', 'objects', 'bin', 'data'):
        (directory/name).mkdir(parents=True, exist_ok=True)
    for path in sorted(example.iterdir()):
        if path.suffix in ('.c', '.h'):
            shutil.copy2(path, directory/'source'/path.name)
    for name in ('llvm-static-dispatch-bench.py', 'llvm_bench.py', 'ppc-static-dispatch.py', 'ppc-dispatch-report.py'):
        shutil.copy2(root/'cmake'/name, directory/'source/tools'/name)
    reader = load_sealer(root)

    def compile_unit(unit, mode, output):
        run(args.compiler, '--ppc-dispatch='+mode, '-S', '-emit-llvm', directory/'source'/(unit+'.c'), '-o', output)
        run(args.llvm_as, output, '-o', output.with_suffix('.bc'))
        run(args.llvm_opt, '-passes=verify', '-disable-output', output.with_suffix('.bc'))

    def optimize(source, output):
        run(args.llvm_opt, '-mcpu=x86-64', '-S', '-passes=default<O2>', '-verify-each', source, '-o', output)
        run(args.llvm_as, output, '-o', output.with_suffix('.bc'))

    def emit(source, output):
        run(args.llvm_llc, '-O=2', '-mcpu=x86-64', '-relocation-model=pic', '-filetype=obj', source, '-o', output)
        run(args.llvm_llc, '-O=2', '-mcpu=x86-64', '-relocation-model=pic', '-filetype=asm', source, '-o', output.with_suffix('.s'))

    timer = directory/'input/timer.ll'
    compile_unit('timer', 'runtime', timer)
    _, timer_families = reader.dispatch.read_module(timer)
    if timer_families or '@llvm.global_ctors' in timer.read_text():
        raise RuntimeError('shared timer must contain no PPC registry or constructors')
    timer_names = set(reader.functions(timer.read_text()))
    optimize(timer, directory/'objects/timer.ll')
    timer_refs = reader.globals_in(reader.functions((directory/'objects/timer.ll').read_text())['main']['body'])
    if timer_refs.count('bench_run') < 2 or timer_refs.count('clock_gettime') != 4:
        raise RuntimeError('warmup/timed calls or clock boundaries disappeared from the shared timer')
    emit(directory/'objects/timer.bc', directory/'objects/timer.o')
    inputs = {}
    for mode in ('runtime', 'switch'):
        folder = directory/'input'/mode
        folder.mkdir(exist_ok=True)
        inputs[mode] = [timer]
        for unit in ('setup', 'kernels', 'defaults', 'specials'):
            path = folder/(unit+'.ll')
            compile_unit(unit, mode, path)
            inputs[mode].append(path)
        kernel_text = (folder/'kernels.ll').read_text()
        catalogs = {name for name in reader.globals_in(kernel_text) if name.startswith('.L.ppc_candidates.')}
        expected_catalogs = {'.L.ppc_candidates.'+name for name in FAMILIES}
        if (mode == 'runtime' and catalogs) or (mode == 'switch' and
                (catalogs != expected_catalogs or kernel_text.count('switch i32') != 4)):
            raise RuntimeError('raw frontend dispatch representation is not the requested one')
    sizes, kernel_sizes, manifests = [], [], {}
    metric_lines = ['pipeline\tfunction\tinstructions\talloca\tload\tstore\tphi\tdirect_calls\tindirect_calls\tmemory_intrinsics\n']
    for variant in VARIANTS:
        print('Building '+variant, flush=True)
        folder = directory/variant
        folder.mkdir(exist_ok=True)
        mode = 'switch' if variant in ('switch', 'static-switch') else 'runtime'
        full = folder/'program.ll'
        if variant.startswith('static-'):
            run(sys.executable, root/'cmake/ppc-static-dispatch.py', '--closed-program', *inputs[mode],
                '--llvm-as', args.llvm_as, '--llvm-opt', args.llvm_opt, '--llvm-link', args.llvm_link,
                '-o', full, '--report', folder/'closure.json')
            closure = json.loads((folder/'closure.json').read_text())
            if set(closure['selected_families']) != FAMILIES:
                raise RuntimeError('static closure did not freeze exactly the four benchmark families')
            if (digest(full) != closure['output_sha256'] or not closure['audit']['removed_runtime'] or
                    set(reader.globals_in(full.read_text())) & set(closure['audit']['removed_runtime'])):
                raise RuntimeError('static lifecycle removal or output provenance mismatch')
            manifests[variant] = closure['manifest_sha256']
        else:
            run(args.llvm_link, '-S', *inputs[mode], '-o', full)
        # The timer was part of the complete IR set used for closure. Lower
        # that same, registry-free unit once; lower the remaining core here.
        # All four programs link its identical object, with no linker LTO.
        core_input = folder/'core-input.ll'
        core_input.write_text(partition(full.read_text(), timer_names, reader))
        run(args.llvm_as, core_input, '-o', folder/'core-input.bc')
        optimize(core_input, folder/'core.ll')
        emit(folder/'core.bc', folder/'core.o')
        passes = run(args.llvm_opt, '-mcpu=x86-64', '-passes=default<O2>', '-print-pipeline-passes',
                     '-disable-output', core_input)
        (folder/'passes.txt').write_text(passes)
        run(args.host_cc, '-fno-lto', directory/'objects/timer.o', folder/'core.o', '-o', directory/'bin'/(variant+'.exe'))
        metric_lines.append(run('awk', '-v', 'pipeline='+variant, '-f', root/'cmake/llvm-ir-metrics.awk', folder/'core.ll'))
        definitions = reader.functions((folder/'core.ll').read_text())
        if set(reader.globals_in(definitions['bench_run']['body'])) & set(KERNELS) != set(KERNELS):
            raise RuntimeError('a noinline batch boundary disappeared from the timed dispatcher')
        for name in KERNELS:
            fn = definitions[name]
            (folder/(name+'.txt')).write_text('define '+fn['header']+' {\n'+fn['body']+'}\n')
        sections, symbols = object_sizes(folder/'core.o')
        if not set(KERNELS) <= symbols.keys():
            raise RuntimeError('measured kernel symbol disappeared')
        kernel_sizes.extend(dict(variant=variant, kernel=name, text_bytes=symbols[name]) for name in KERNELS)
        executable_sections, _ = object_sizes(directory/'bin'/(variant+'.exe'))
        sizes.append(dict(variant=variant, core_text_bytes=text_size(sections),
            kernels_text_bytes=sum(symbols[name] for name in KERNELS),
            executable_text_bytes=text_size(executable_sections),
            core_rodata_bytes=sum(size for section, size in sections.items() if section.startswith(('.rodata', '.data.rel.ro'))),
            executable_file_bytes=(directory/'bin'/(variant+'.exe')).stat().st_size))
    (directory/'metrics.tsv').write_text(''.join(metric_lines))
    with (directory/'code-sizes.tsv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(sizes[0]), delimiter='\t')
        writer.writeheader(); writer.writerows(sizes)
    with (directory/'kernel-sizes.tsv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(kernel_sizes[0]), delimiter='\t')
        writer.writeheader(); writer.writerows(kernel_sizes)
    try:
        head = run('git', '-C', root, 'rev-parse', 'HEAD').strip()
        dirty = bool(run('git', '-C', root, 'status', '--porcelain').strip())
    except RuntimeError:
        head, dirty = None, None
    info = dict(llvm_versions=versions, host_cc_version=run(args.host_cc, '--version'),
        compiler_sha256=digest(args.compiler), repository_head=head, working_tree_dirty=dirty,
        cpu=run('lscpu') if shutil.which('lscpu') else platform.processor(),
        shared_timer_sha256=digest(directory/'objects/timer.o'), code_sizes=sizes, kernel_code_sizes=kernel_sizes,
        binutils_versions={tool: run(tool, '--version').splitlines()[0] for tool in ('size', 'nm')},
        closure_manifests=manifests, kernel_boundary='four noinline batch kernels; handler inlining enabled',
        partition='complete program includes timer IR; same timer unit lowered once into a shared object; no linker LTO',
        commands=COMMANDS, source_sha256={str(path.relative_to(directory)): digest(path)
            for path in sorted((directory/'source').rglob('*')) if path.is_file()})
    (directory/'build.json').write_text(json.dumps(info, indent=2)+'\n')
    return info


def data_seed(config, mode):
    return (config['seed'] ^ 0xc001d00d ^ ((mode//4)*0x9e3779b9) ^ ((mode%2)*0x85ebca6b)) & 0xffffffff


def records(mode, seed, count):
    arity, hot = 2+mode//4, mode%2
    cells = 6 if arity == 2 else 12
    dominant, state, result = count*9//10, seed, []
    def next_number():
        nonlocal state
        state = (1664525*state+1013904223) & 0xffffffff
        return state
    for i in range(count):
        flat = (cells-1 if i < dominant else (i-dominant) % (cells-1)) if hot else i % cells
        result.append((flat//3 if arity == 2 else flat//6,
                       flat%3 if arity == 2 else flat//2%3, 0 if arity == 2 else flat%2, next_number() >> 16))
    for i in range(count-1, 0, -1):
        j = next_number() % (i+1)
        result[i], result[j] = result[j], result[i]
    return result


@lru_cache(maxsize=None)
def reference(mode, seed, count):
    rows = records(mode, seed, count)
    total, specific, fingerprint = 0, 0, 14695981039346656037
    histogram = [0]*(6 if mode < 4 else 12)
    for a, b, c, x in rows:
        flat = a*3+b if mode < 4 else a*6+b*2+c
        histogram[flat] += 1
        concrete = flat in (0, len(histogram)-1)
        specific += concrete
        factor = flat+(101 if concrete else 37)
        value = (x+1)*factor+13
        if mode//2%2:
            value = x+factor
            for step in range(6):
                value = ((value ^ (value >> 13))*0x9e3779b185ebca87) & MASK
                value = ((value ^ (value >> 17))+factor+step) & MASK
            value &= 0xffffff
        total += value
        for word in (a, b, c, x):
            fingerprint = ((fingerprint ^ word)*1099511628211) & MASK
    return dict(per_round_checksum=total, concrete_per_round=specific, default_per_round=count-specific,
        dataset_fingerprint=fingerprint, histogram=histogram,
        dataset_sha256=hashlib.sha256(b''.join(struct.pack('<IIIQ', *row) for row in rows)).hexdigest())


def execute(directory, variant, mode, rounds, seed, count=BATCH):
    command = [str(directory/'bin'/(variant+'.exe')), str(mode), str(rounds), str(seed), str(count)]
    process = subprocess.run(command, text=True, capture_output=True, timeout=120)
    if process.returncode or process.stderr:
        raise RuntimeError(f'{command}: exit {process.returncode}\n{process.stdout}{process.stderr}')
    result = json.loads(process.stdout)
    expected = reference(mode, seed, count)
    fields = dict(mode=mode, rounds=rounds, seed=seed, count=count, calls=rounds*count, warmup_rounds=WARMUP,
        checksum=expected['per_round_checksum']*rounds, warmup_checksum=expected['per_round_checksum']*WARMUP,
        concrete=expected['concrete_per_round']*(rounds+WARMUP), defaults=expected['default_per_round']*(rounds+WARMUP),
        dataset_fingerprint=expected['dataset_fingerprint'])
    if any(result.get(key) != value for key, value in fields.items()):
        raise RuntimeError('independent reference mismatch: '+str(result))
    for field in ('wall_seconds', 'cpu_seconds'):
        if not math.isfinite(result[field]) or result[field] <= 0:
            raise RuntimeError('invalid duration: '+str(result))
    result.update(variant=variant, workload=WORKLOADS[mode], dataset_sha256=expected['dataset_sha256'])
    return result


def checks(directory, config):
    results = []
    for variant in VARIANTS:
        for mode in range(len(WORKLOADS)):
            for count, seed in ((0, 0), (1, 0xffffffff), (17, config['seed']), (BATCH, data_seed(config, mode))):
                for rounds in (1, 3):
                    results.append(execute(directory, variant, mode, rounds, seed, count))
        for args in (('-1', '1', '0', '1'), ('8', '1', '0', '1'), ('0', '0', '0', '1'),
                     ('0', '1000001', '0', '1'), ('0', '1', '-1', '1'), ('0', '1', '4294967296', '1'),
                     ('0', '1', '0', '4097'), ('bad', '1', '0', '1'), ('0', 'bad', '0', '1'),
                     ('0', '1', 'bad', '1'), ('0', '1', '0', 'bad'), ('0', '1'), ('0', '1', '0', '1', 'extra')):
            process = subprocess.run([str(directory/'bin'/(variant+'.exe')), *args], text=True, capture_output=True, timeout=10)
            if process.returncode != 2 or process.stdout or not process.stderr.startswith('usage:'):
                raise RuntimeError('invalid-argument check failed: '+str(args))
    (directory/'checks.json').write_text(json.dumps(results, indent=2)+'\n')
    return results


def make_report(directory, info, samples):
    config = info['settings']
    summary = summarize(samples, VARIANTS, WORKLOADS)
    write_tables(directory, samples, summary)
    lines = ['# Закрытые мультиметоды: runtime, switch и статические таблицы', '',
        f"UTC: {info['utc']}. Повторов: {config['samples']}; цель калибровки: {config['seconds']} с; seed: {config['seed']}.",
        f"Affinity: {info['affinity']['effective']}. Все проверки результата и счётчиков пройдены.", '',
        'Медиана нс/вызов; в скобках MAD в % от медианы. Отношение > 1 означает ускорение статического варианта того же представления.', '',
        '| Нагрузка | runtime | switch | static-table | static-switch | runtime/static-table | switch/static-switch |',
        '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    for mode, workload in enumerate(WORKLOADS):
        cells = [f"{summary[mode, v]['median_ns']:.3f} ({summary[mode, v]['mad_percent']:.1f}%)" for v in VARIANTS]
        ratio = summary[mode, 'runtime']['median_ns']/summary[mode, 'static-table']['median_ns']
        switch_ratio = summary[mode, 'switch']['median_ns']/summary[mode, 'static-switch']['median_ns']
        lines.append(f"| {workload} | {' | '.join(cells)} | {ratio:.3f} | {switch_ratio:.3f} |")
    lines += ['', '## Размер машинного кода', '',
        '| Вариант | Ядро .text, байт | Четыре пакетных функции, байт | Программа .text, байт | Данные ядра .rodata/.data.rel.ro, байт |',
        '| --- | ---: | ---: | ---: | ---: |']
    for row in info['build']['code_sizes']:
        lines.append(f"| {row['variant']} | {row['core_text_bytes']} | {row['kernels_text_bytes']} | {row['executable_text_bytes']} | {row['core_rodata_bytes']} |")
    lines += ['', 'Матрицы: 2×3 и 2×3×2. uniform покрывает все координаты; hot90 отдаёт примерно 90% последней конкретной комбинации.',
        'Два конкретных обработчика занимают нулевую и последнюю координаты; остальные ячейки используют общий обработчик. Точные частоты — datasets.json.', '',
        'short — короткая целочисленная формула; work — шесть шагов смешивания unsigned 64-bit. Обработчики изменяют счётчики конкретных/общих вызовов.', '',
        'Измеряется цикл пакетных вызовов: пакет содержит 4096 динамических диспетчеризаций. Создание процесса, конструкторы, подготовка и эталон, восемь прогревов и печать исключены.',
        'Таймер — один LLVM-объект во всех программах. Его исходный IR включён в полный набор при статическом преобразовании, затем та же единица снижена отдельно. Линковка без LTO.',
        'Четыре пакетные функции имеют noinline; обработчики доступны для встраивания. LLVM может сокращать проверки и обновления счётчиков внутри ядра.', '',
        'Сравниваются полные pipeline: статический режим также удаляет конструкторы и меняет видимость обработчиков. Разность времени не изолирует стоимость одной загрузки либо ветви.',
        'Это микробенчмарк с горячими данными; результаты относятся к данной машине и входам. MAD описывает разброс, а не доверительный интервал.', '']
    noisy = sum(row['mad_percent'] > 10 for row in summary.values())
    waiting = sum(row['cpu_seconds']/row['wall_seconds'] < .8 for row in samples)
    if noisy or waiting:
        lines += [f'Рядов с MAD > 10%: {noisy}; замеров с CPU/wall < 0.8: {waiting}. Учитывайте шум при сравнении близких значений.', '']
    lines += ['Артефакты: samples.json/tsv, summary.tsv, datasets.json и data/, code-sizes.tsv, kernel-sizes.tsv, metrics.tsv, build.json, исходный/оптимизированный IR, ассемблер и объекты каждого варианта.', '']
    (directory/'report.md').write_text('\n'.join(lines), encoding='utf-8')


def main():
    if sys.argv[1:] == ['--check-config']:
        settings()
        return
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-dir', 'compiler', 'host-cc', 'llvm-as', 'llvm-opt', 'llvm-llc', 'llvm-link', 'result-dir'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    directory = args.result_dir.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    for name in ('report.md', 'samples.json', 'samples.tsv', 'summary.tsv', 'checks.json', 'check-info.json'):
        (directory/name).unlink(missing_ok=True)
    config = settings()
    selected_cpu = affinity(config['cpu'])
    info = dict(utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), settings=config,
                affinity=selected_cpu, platform=platform.platform(), python=sys.version,
                batch_calls=BATCH, warmup_rounds=WARMUP, build=build(args, directory))
    info['executable_sha256'] = {v: digest(directory/'bin'/(v+'.exe')) for v in VARIANTS}
    info['datasets'] = {}
    for mode, workload in enumerate(WORKLOADS):
        seed = data_seed(config, mode)
        info['datasets'][workload] = dict(reference(mode, seed, BATCH), seed=seed, count=BATCH)
        rows = records(mode, seed, BATCH)
        snapshot = directory/'data'/(workload+'.tsv')
        snapshot.write_text('a\tb\tc\tx\n'+''.join('\t'.join(map(str, row))+'\n' for row in rows))
        info['datasets'][workload]['snapshot_tsv_sha256'] = digest(snapshot)
    (directory/'datasets.json').write_text(json.dumps(info['datasets'], indent=2)+'\n')
    print('Checking all variants against independent scalar/Python references', flush=True)
    info['checks'] = checks(directory, config)
    for variant, checksum in info['executable_sha256'].items():
        if digest(directory/'bin'/(variant+'.exe')) != checksum:
            raise RuntimeError('executable changed during checks: '+variant)
    if args.check_only:
        (directory/'check-info.json').write_text(json.dumps(info, indent=2)+'\n')
        print('Static dispatch benchmark checks passed; no performance samples requested', flush=True)
        return
    samples = measure(directory, info, VARIANTS, WORKLOADS,
        lambda variant, mode, rounds: execute(directory, variant, mode, rounds, data_seed(config, mode)), MAX_ROUNDS)
    if digest(directory/'objects/timer.o') != info['build']['shared_timer_sha256']:
        raise RuntimeError('shared timer changed during measurement')
    for workload, record in info['datasets'].items():
        if digest(directory/'data'/(workload+'.tsv')) != record['snapshot_tsv_sha256']:
            raise RuntimeError('dataset snapshot changed during measurement: '+workload)
    make_report(directory, info, samples)
    print('Static dispatch benchmark passed; report: '+str(directory/'report.md'), flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError, OSError, KeyError, subprocess.TimeoutExpired) as error:
        sys.exit('LLVM static dispatch benchmark: '+str(error))
