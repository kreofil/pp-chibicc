#!/usr/bin/env python3
"""Verify LLVM bootstrap generations and save reproducible comparison results."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_report(out, info):
    (out/'report.json').write_text(json.dumps(info, indent=2, ensure_ascii=False)+'\n')
    lines = ['# LLVM self-hosting', '', 'Status: **'+info['status']+'**', '',
             'Optimization: `-O'+info['optimization']+'`.', '',
             'Stage1 emits stage2; LLVM-built stage2 emits stage3. Both use the same LLVM tools.', '',
             '| Source | Raw IR identical | Objects identical | Object SHA256 |',
             '| --- | --- | --- | --- |']
    for item in info['comparisons']:
        lines.append('| {} | {} | {} | `{}` |'.format(item['source'],
            item['ir_identical'], item['object_identical'], item['stage3_object_sha256'][:16]))
    lines += ['', 'Generation checks: **{}**. Corpus comparisons: **{}**. Smoke executions: **{}**.'.format(
        len(info['comparisons']), len(info['corpus']), len(info['smoke'])), '',
        'This report covers bootstrap identity and focused checks. The full C/4P, LLVM and closed-program',
        'suites are separate dependencies of the CMake target `test-llvm-selfhost`.', '',
        'Tool versions, compiler/input hashes and commands are in `report.json`.']
    if 'error' in info:
        lines += ['', 'Error: '+info['error']]
    (out/'report.md').write_text('\n'.join(lines)+'\n')


def verify(args, out, info):
    root = args.source_dir.resolve()
    build = args.build_dir.resolve()
    env = dict(os.environ, LC_ALL='C', PYTHONDONTWRITEBYTECODE='1',
               PPCHIBICC_LLVM_OPT=args.llvm_opt, PPCHIBICC_LLVM_LLC=args.llvm_llc)

    def run(*command, status=0):
        command = [str(a) for a in command]
        entry = {'argv': command}
        info['commands'].append(entry)
        start = time.monotonic()
        p = subprocess.run(command, cwd=root, env=env, text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
        entry.update(status=p.returncode, seconds=time.monotonic()-start,
                     stdout=p.stdout, stderr=p.stderr)
        if p.returncode != status:
            raise RuntimeError('{}: exit {}, expected {}\n{}{}'.format(
                command, p.returncode, status, p.stdout, p.stderr))
        return p

    majors = []
    info['host_cc'] = {'path': args.host_cc, 'version': run(args.host_cc, '--version').stdout,
                       'sha256': digest(args.host_cc)}
    for name in ('as', 'opt', 'llc'):
        path = getattr(args, 'llvm_'+name)
        version = run(path, '--version').stdout
        match = re.search(r'LLVM version (\d+)\.', version)
        if not match:
            raise RuntimeError('Cannot determine LLVM version: '+path)
        majors.append(int(match[1]))
        info['llvm_tools'][name] = {'path': path, 'version': version, 'sha256': digest(path)}
    if min(majors) < 19 or len(set(majors)) != 1:
        raise RuntimeError('Matching llvm-as/opt/llc versions 19+ are required')

    compilers = {'seed': args.seed_compiler.resolve(),
                 'stage2': build/'stage2/ppchibicc', 'stage3': build/'stage3/ppchibicc'}
    for name, compiler in compilers.items():
        run(compiler, '--help')
        if run(compiler, '-hashmap-test').stdout != 'OK\n':
            raise RuntimeError(name+': hashmap check output differs')
        info['compilers'][name] = {'path': str(compiler), 'sha256': digest(compiler),
                                  'bytes': compiler.stat().st_size}

    info['inputs']['src/chibicc.h'] = digest(root/'src/chibicc.h')
    for source in ('CMakeLists.txt', 'cmake/llvm-selfhost-report.py'):
        info['inputs'][source] = digest(root/source)
    for path in sorted((root/'include').rglob('*')):
        if path.is_file():
            info['inputs'][str(path.relative_to(root))] = digest(path)
    for source in args.sources:
        stem = Path(source).stem
        raw2, raw3 = (build/stage/'ir'/(stem+'.ll') for stage in ('stage2', 'stage3'))
        obj2, obj3 = (build/stage/'obj'/(stem+'.o') for stage in ('stage2', 'stage3'))
        item = {'source': source, 'ir_identical': raw2.read_bytes() == raw3.read_bytes(),
                'object_identical': obj2.read_bytes() == obj3.read_bytes(),
                'stage2_ir_sha256': digest(raw2), 'stage3_ir_sha256': digest(raw3),
                'stage2_object_sha256': digest(obj2), 'stage3_object_sha256': digest(obj3)}
        info['comparisons'].append(item)
        info['inputs'][source] = digest(root/source)
        bc = out/(stem+'.bc')
        run(args.llvm_as, raw3, '-o', bc)
        run(args.llvm_opt, '-passes=verify', '-disable-output', bc)
    if not all(x['ir_identical'] and x['object_identical'] for x in info['comparisons']):
        raise RuntimeError('Stage2/stage3 bootstrap outputs differ; inspect comparison hashes')
    info['executables_identical'] = digest(compilers['stage2']) == digest(compilers['stage3'])

    corpus = ['test/bool-keywords.c', 'test/ppc-bitfield.c', 'test/ppc-vla-params.c',
              'test/ppc-vararg-records.c', 'test/llvm/valid/stdarg-records.c']
    for index, source in enumerate(corpus):
        info['inputs'][source] = digest(root/source)
        for kind, flags, suffix in [('llvm', ['-S', '-emit-llvm'], 'll'), ('native', ['-S'], 's')]:
            outputs = []
            for name, compiler in compilers.items():
                path = out/('corpus-{}-{}-{}.{}'.format(index, kind, name, suffix))
                run(compiler, *flags, '-fno-common', '-I'+str(root/'include'),
                    '-I'+str(root/'test'), source, '-o', path)
                outputs.append(digest(path))
            result = {'source': source, 'kind': kind, 'identical': len(set(outputs)) == 1,
                      'sha256': dict(zip(compilers, outputs))}
            info['corpus'].append(result)
            if not result['identical']:
                raise RuntimeError('Generated '+kind+' differs between compilers: '+source)

    for name in ('stage2', 'stage3'):
        compiler = compilers[name]
        for stem in ('hello', 'ppc-bitfields', 'ppc-vararg-items', 'ppc-vla-matrix'):
            for backend in ('x86-64', 'llvm'):
                exe = out/(name+'-'+backend+'-'+stem+'.exe')
                run(compiler, '--backend='+backend, '-O2', 'examples/llvm/'+stem+'.c', '-o', exe)
                p = run(exe)
                expected = (root/'test/llvm/valid'/(stem+'.expected')).read_text()
                if p.stdout != expected or p.stderr:
                    raise RuntimeError('Unexpected smoke output: '+str(exe))
                info['smoke'].append({'compiler': name, 'backend': backend,
                                      'source': stem, 'stdout': p.stdout})

    bad = out/'invalid.c'
    bad.write_text('int main(void) { return missing_variable; }\n')
    for name, compiler in compilers.items():
        p = run(compiler, '-S', '-emit-llvm', bad, '-o', out/(name+'-invalid.ll'), status=1)
        if 'undefined variable' not in p.stderr:
            raise RuntimeError('Missing invalid-source diagnostic from '+name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-dir', 'build-dir', 'seed-compiler'):
        parser.add_argument('--'+name, type=Path, required=True)
    for name in ('llvm-as', 'llvm-opt', 'llvm-llc', 'host-cc'):
        parser.add_argument('--'+name, required=True)
    parser.add_argument('--optimization', choices=('0', '1', '2', '3', 's', 'z'), required=True)
    parser.add_argument('--sources', nargs='+', required=True)
    args = parser.parse_args()
    out = args.build_dir.resolve()/'llvm-selfhost'
    out.mkdir(parents=True, exist_ok=True)
    info = {'schema': 1, 'status': 'running', 'optimization': args.optimization,
            'platform': {'system': platform.system(), 'machine': platform.machine(),
                         'libc': platform.libc_ver()},
            'compilers': {}, 'llvm_tools': {}, 'inputs': {}, 'comparisons': [],
            'corpus': [], 'smoke': [], 'commands': []}
    write_report(out, info)
    try:
        verify(args, out, info)
        info['status'] = 'passed'
    except Exception as error:
        info.update(status='failed', error=str(error))
        print('LLVM self-hosting failed: '+str(error), file=sys.stderr)
    write_report(out, info)
    print('LLVM self-hosting '+info['status']+'; report: '+str(out/'report.md'))
    return 0 if info['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
