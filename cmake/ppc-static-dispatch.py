#!/usr/bin/env python3
"""Freeze resolved all-closed PPC tables in an explicitly closed LLVM program.

Inputs must be raw ppchibicc LLVM translation units with v2 dispatch metadata.
The explicit closed-program contract includes every user module; unresolved
references are restricted to the supported C runtime and LLVM intrinsics.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("ppc_dispatch_report", Path(__file__).with_name("ppc-dispatch-report.py"))
dispatch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dispatch)

NAME = r'@(?:"(?:\\[0-9A-Fa-f]{2}|[^"\\])*"|[-a-zA-Z$._0-9]+)'
TOKENS = re.compile(NAME + r'|"(?:\\[0-9A-Fa-f]{2}|[^"\\])*"|;[^\n]*')
LOCAL = re.compile(r'%[-a-zA-Z$._0-9]+')
FUNCTION = re.compile(r'^define ([^\n]+) \{\n(.*?)^\}\n', re.M | re.S)
ENTRY = re.compile(r'\{ i32, ptr, ptr \} \{ i32 (\d+), ptr ('+NAME+r'), ptr null \}')
ROLES = {"allocate": ("ctors", 201), "release": ("dtors", 201),
         "handler": ("ctors", 301), "default": ("ctors", 301), "check": ("ctors", 351)}
# A bounded first executable mode: application helpers must be supplied as IR.
# Dynamic loading APIs and arbitrary native application libraries are excluded.
C_FUNCTIONS = set("""abort exit _Exit _exit atexit __assert_fail __errno_location
malloc calloc realloc reallocarray free aligned_alloc posix_memalign
memcpy memmove memset memcmp memchr strlen strnlen strcmp strncmp strcpy strncpy
strcat strncat strchr strrchr strstr strdup strndup atoi atol atoll strtol strtoul
strtoll strtoull strtod strtof strtold abs labs llabs
printf fprintf sprintf snprintf vprintf vfprintf vsprintf vsnprintf dprintf vdprintf
puts putchar fputs fputc getchar fgets fgetc fwrite fread fflush fopen fclose
fseek ftell rewind feof ferror clearerr perror remove rename qsort bsearch
time clock clock_gettime gettimeofday rand srand read write close open lseek
getenv setenv unsetenv isatty getpid sleep usleep nanosleep
sqrt sqrtf sqrtl sin cos tan exp log pow fabs fabsf fabsl floor ceil fmod
""".split())
C_DATA = {"stdin", "stdout", "stderr", "environ"}
TARGET = 'x86_64-pc-linux-gnu'
LAYOUT = 'e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128'
CLOSED_OBJECT_HEADER = b'; ppchibicc closed-program object v1\n'


def symbol(token):
    return dispatch.unescape(token[2:-1]) if token.startswith('@"') else token[1:]


def quoted(name):
    return '@"' + ''.join('\\%02X' % byte for byte in name.encode('utf-8')) + '"'


def globals_in(text):
    return [symbol(m[0]) for m in TOKENS.finditer(text) if m[0].startswith('@')]


def code(line):
    for match in TOKENS.finditer(line):
        if match[0].startswith(';'):
            return line[:match.start()]
    return line


def functions(text):
    result = {}
    for match in FUNCTION.finditer(text):
        name = globals_in(match[1])[0]
        if name in result:
            raise ValueError("duplicate function definition " + name)
        result[name] = dict(header=match[1], body=match[2], span=match.span())
    return result


def metadata(text):
    root = re.search(r'^!ppchibicc\.dispatch = !\{([^\n]*)\}$', text, re.M)
    if not root:
        raise ValueError("dispatch metadata root is missing")
    nodes = dict(re.findall(r'^!(\d+) = !\{([^\n]*)\}$', text, re.M))
    return [(ident, dispatch.fields(nodes[ident])) for ident in re.findall(r'!(\d+)', root[1])]


def initializers(text):
    arrays = {}
    for kind in ("ctors", "dtors"):
        match = re.search(r'^@llvm\.global_'+kind+r' = appending global \[(\d+) x \{ i32, ptr, ptr \}\] \[([^\n]*)\]$', text, re.M)
        if not match:
            if re.search(r'^@llvm\.global_'+kind+r'\b', text, re.M):
                raise ValueError("unsupported initializer array " + kind)
            arrays[kind] = []
            continue
        entries = list(ENTRY.finditer(match[2]))
        if len(entries) != int(match[1]) or ENTRY.sub('', match[2]).strip(' ,'):
            raise ValueError("unsupported initializer entries " + kind)
        arrays[kind] = [(int(entry[1]), symbol(entry[2]), entry[0]) for entry in entries]
    return arrays


def audit_runtime(text, families):
    fns = functions(text)
    arrays = initializers(text)
    owners = {}
    for family in families:
        expected = Counter(allocate=1, release=1,
            handler=sum(h["coordinates"] is not None for h in family["handlers"]),
            default=int(any(h["coordinates"] is None for h in family["handlers"])),
            check=int(not any(h["coordinates"] is None for h in family["handlers"])))
        actual = Counter(r["role"] for r in family["runtime"])
        if actual != +expected:
            raise ValueError("incomplete runtime description for " + family["name"])
        targets = {h["symbol"] for h in family["handlers"]}
        for runtime in family["runtime"]:
            name, role = runtime["symbol"], runtime["role"]
            if name in owners or name not in fns or role not in ROLES:
                raise ValueError("invalid runtime function " + str(name))
            fn = fns[name]
            if not re.fullmatch(r'internal void '+NAME+r'\(\)', fn["header"]):
                raise ValueError("runtime function is not an internal void function: " + name)
            kind, priority = ROLES[role]
            occurrences = [(k, p) for k, entries in arrays.items() for p, n, _ in entries if n == name]
            if occurrences != [(kind, priority)]:
                raise ValueError("runtime initializer priority/ownership mismatch: " + name)
            allowed_calls = {"allocate": {"calloc", "dprintf", "abort"}, "release": {"free"},
                             "check": {"dprintf", "abort"}, "handler": set(), "default": set()}[role]
            for line in fn["body"].splitlines():
                refs = set(globals_in(line))
                allowed = targets | {family["table"]} | allowed_calls
                if role in ("allocate", "check"):
                    allowed |= {ref for ref in refs if ref.startswith('.L..')}
                if not refs <= allowed:
                    raise ValueError("unaccounted runtime side effect: " + name)
                if re.search(r'\bcall\b', code(line)) and (not refs or globals_in(line)[0] not in allowed_calls):
                    raise ValueError("unexpected call in runtime function: " + name)
            owners[name] = family["name"]
    return owners


def audit_reads(body, tables):
    # Reject stores and escapes of either the table cell address or its data
    # pointer. Loading a handler from a table slot ends address propagation.
    instructions = [code(line).strip() for line in body.splitlines()]
    tainted = set()
    roots = set()
    for line in instructions:
        refs = set(globals_in(line)) & tables
        if refs:
            match = re.fullmatch(r'(%[-a-zA-Z$._0-9]+) = load ptr, ptr ('+NAME+r'), align \d+', line)
            if not match or refs != {symbol(match[2])}:
                raise ValueError("dispatch table address is written or escapes: " + ', '.join(sorted(refs)))
            tainted.add(match[1])
            roots.add(line)
    allowed = {"getelementptr", "ptrtoint", "inttoptr", "bitcast", "add", "sub", "mul",
               "shl", "lshr", "ashr", "and", "or", "xor", "select", "phi"}
    progress = True
    while progress:
        progress = False
        for line in instructions:
            assignment = re.match(r'(%[-a-zA-Z$._0-9]+) = (\w+)\b(.*)', line)
            if not assignment or assignment[2] not in allowed:
                continue
            if set(LOCAL.findall(assignment[3])) & tainted and assignment[1] not in tainted:
                tainted.add(assignment[1])
                progress = True
    for line in instructions:
        if line in roots:
            continue
        assignment = re.match(r'(%[-a-zA-Z$._0-9]+) = (.*)', line)
        rhs = assignment[2] if assignment else line
        used = set(LOCAL.findall(rhs)) & tainted
        if not used:
            continue
        op = rhs.split()[0]
        if assignment and op in allowed:
            continue
        if assignment and re.fullmatch(r'load ptr, ptr %[-a-zA-Z$._0-9]+, align \d+', rhs):
            continue
        raise ValueError("dispatch table data is written, observed or escapes: " + line)


def audit_uses(text, tables, removed):
    fns = functions(text)
    outside = text
    for name, fn in sorted(fns.items(), key=lambda item: item[1]["span"][0], reverse=True):
        if name not in removed:
            audit_reads(fn["body"], tables)
            if set(globals_in(fn["body"])) & set(removed):
                raise ValueError("runtime initializer is called outside startup: " + name)
        start, end = fn["span"]
        outside = outside[:start] + outside[end:]
    for line in outside.splitlines():
        refs = set(globals_in(line))
        if not refs & (tables | set(removed)) or line.startswith('!'):
            continue
        if not refs & tables and (line.startswith('@llvm.global_ctors =') or line.startswith('@llvm.global_dtors =')):
            continue
        if re.fullmatch('('+NAME+r') = common global \[8 x i8\] zeroinitializer, align 8', line):
            if globals_in(line)[0] in tables and len(refs) == 1:
                continue
        raise ValueError("unaccounted global reference to dispatch state: " + line[:160])


def check_externals(text):
    externals = []
    for line in text.splitlines():
        if line.startswith('declare '):
            refs = globals_in(line)
            if not refs:
                raise ValueError("unsupported external declaration")
            name = refs[0]
            if name not in C_FUNCTIONS and not name.startswith('llvm.'):
                raise ValueError("external function requires a user LLVM module: " + name)
            externals.append(name)
        elif re.match('^'+NAME+r' = (?:external|extern_weak) ', line):
            name = globals_in(line)[0]
            if name not in C_DATA:
                raise ValueError("external data requires a user LLVM module: " + name)
            externals.append(name)
    return sorted(set(externals))


def transform(text, module_path, families, entry):
    chosen = {family["name"] for family in families}
    _, descriptors = dispatch.read_module(module_path)
    selected = [f for f in descriptors if f["name"] in chosen]
    removed = audit_runtime(text, selected)
    tables = {family["table"] for family in families}
    audit_uses(text, tables, removed)
    fns = functions(text)
    if entry not in fns:
        raise ValueError("program entry definition is missing: " + entry)
    externals = check_externals(text)
    targets = {cell["handlers"][0]["symbol"] for f in families for cell in f["cells"]}
    edits = []
    for name, fn in fns.items():
        start, end = fn["span"]
        if name in removed:
            edits.append((start, end, ''))
        elif name in targets and not fn["header"].startswith('internal '):
            if re.search(r'\b(weak|linkonce|available_externally|external|comdat)\b', fn["header"]):
                raise ValueError("unsupported handler linkage: " + name)
            edits.append((start, start+len('define '), 'define internal '))
    arrays = initializers(text)
    for kind, entries in arrays.items():
        match = re.search(r'^@llvm\.global_'+kind+r' = [^\n]*\n', text, re.M)
        if not match:
            continue
        kept = [raw for _, name, raw in entries if name not in removed]
        replacement = ('@llvm.global_'+kind+' = appending global ['+str(len(kept))+
            ' x { i32, ptr, ptr }] ['+', '.join(kept)+']\n') if kept else ''
        edits.append((match.start(), match.end(), replacement))
    for family in families:
        table = family["table"]
        token = next(m[0] for m in TOKENS.finditer(text) if m[0].startswith('@') and symbol(m[0]) == table)
        match = re.search('^'+re.escape(token)+r' = common global \[8 x i8\] zeroinitializer, align 8\n', text, re.M)
        if not match:
            raise ValueError("dispatch table is not an unmodified common cell: " + table)
        array = '.L.ppc_static.'+family["name"]
        if array in globals_in(text):
            raise ValueError("static table name collision: " + array)
        values = ', '.join('ptr '+quoted(cell["handlers"][0]["symbol"]) for cell in family["cells"])
        definition = (quoted(array)+' = private unnamed_addr constant ['+str(family["cell_count"])+
            ' x ptr] ['+values+'], align 8\n'+quoted(table)+' = internal constant ptr '+quoted(array)+', align 8\n')
        edits.append((match.start(), match.end(), definition))
    records = metadata(text)
    remove_ids = {ident for ident, record in records if record[1] == ('string', 'runtime') and record[3][1] in chosen}
    for ident in remove_ids:
        match = re.search('^!'+ident+r' = [^\n]*\n', text, re.M)
        edits.append((match.start(), match.end(), ''))
    root = re.search(r'^!ppchibicc\.dispatch = !\{[^\n]*\}\n', text, re.M)
    edits.append((root.start(), root.end(), '!ppchibicc.dispatch = !{'+', '.join('!'+i for i, _ in records if i not in remove_ids)+'}\n'))
    for start, end, replacement in sorted(edits, reverse=True):
        text = text[:start]+replacement+text[end:]
    return text, dict(removed_runtime=sorted(removed), sealed_handlers=sorted(targets),
                      external_runtime_symbols=externals)


def run(tool, *args):
    result = subprocess.run([str(tool), *map(str, args)], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise ValueError(str(tool)+" failed:\n"+result.stderr)
    return result.stdout


def atomic_outputs(outputs):
    prepared = []
    try:
        for path, data in outputs:
            if path.is_dir():
                raise ValueError('output is a directory: '+str(path))
            fd, temporary = tempfile.mkstemp(prefix='.'+path.name+'.', dir=path.parent)
            prepared.append((temporary, path))
            with os.fdopen(fd, 'w', encoding='utf-8') as stream:
                stream.write(data)
        for temporary, path in prepared:
            os.replace(temporary, path)
    finally:
        for temporary, _ in prepared:
            if os.path.exists(temporary):
                os.unlink(temporary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('modules', nargs='+', type=Path)
    parser.add_argument('--closed-program', action='store_true', help='assert these are all user program modules')
    parser.add_argument('--allow-empty', action='store_true', help='permit a complete program without eligible tables')
    parser.add_argument('--quiet', action='store_true')
    parser.add_argument('--family', action='append', default=[], help='require transformation of this family')
    parser.add_argument('--entry', default='main')
    parser.add_argument('-o', '--output', required=True, type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--max-cells', type=int, default=4096)
    for name in ('as', 'opt', 'link'):
        parser.add_argument('--llvm-'+name, default=os.environ.get('PPCHIBICC_LLVM_'+name.upper(), 'llvm-'+name if name != 'opt' else 'opt'))
    parser.add_argument('--llvm-llc', help='also check the machine-code tool version when supplied by the compiler')
    args = parser.parse_args()
    try:
        if not args.closed_program:
            raise ValueError('--closed-program must explicitly declare the complete user module set')
        if args.max_cells < 1:
            raise ValueError('--max-cells must be positive')
        report_path = args.report or Path(str(args.output)+'.json')
        paths = [path.resolve() for path in args.modules]
        outputs = [args.output.resolve(), report_path.resolve()]
        if len(set(paths)) != len(paths) or len(set(outputs)) != 2 or set(paths) & set(outputs):
            raise ValueError('inputs and outputs must be distinct')
        versions = []
        for tool in [args.llvm_as, args.llvm_opt, args.llvm_link]+([args.llvm_llc] if args.llvm_llc else []):
            match = re.search(r'LLVM version (\d+)\.', run(tool, '--version'))
            if not match:
                raise ValueError('cannot determine LLVM version: '+tool)
            versions.append(int(match[1]))
        if len(set(versions)) != 1 or versions[0] < 19:
            raise ValueError('LLVM tools must have the same major version, at least 19')
        inputs = []
        for path in paths:
            raw = path.read_bytes()
            closed_object = raw.startswith(CLOSED_OBJECT_HEADER)
            if path.suffix != '.ll' and not (path.suffix == '.o' and closed_object):
                raise ValueError('only raw .ll units or ppchibicc closed-program .o objects are supported: '+str(path))
            text = raw.decode('utf-8')
            if not closed_object and not text.startswith('; ppchibicc experimental LLVM IR backend '):
                raise ValueError('input must be raw ppchibicc IR before optimization/linking: '+str(path))
            if ('target triple = "'+TARGET+'"\n' not in text or
                    'target datalayout = "'+LAYOUT+'"\n' not in text):
                raise ValueError('input must use the supported Linux x86-64 target and data layout: '+str(path))
            records = metadata(text)
            if any(record[0] != ('string', 'ppchibicc.dispatch.v2') for _, record in records):
                raise ValueError('static dispatch requires v2 runtime metadata: '+str(path))
            units, _ = dispatch.read_module(path)
            source = re.search(r'^source_filename = '+dispatch.STRING+r'$', text, re.M)
            if len(units) != 1 or not source or units != {dispatch.unescape(source[1])}:
                raise ValueError('input must describe exactly its own translation unit: '+str(path))
            inputs.append(dict(path=str(path), source=next(iter(units)), sha256=hashlib.sha256(raw).hexdigest()))
        raw_report = dispatch.collect(paths, args.max_cells)
        requested = set(args.family)
        eligible = {f['name'] for f in raw_report['functions'] if f['axes'] and f['status'] == 'resolved'}
        if requested - eligible:
            statuses = {f['name']: f['status'] for f in raw_report['functions']}
            raise ValueError('requested families are not fully closed and resolved: '+', '.join(
                name+' ('+statuses.get(name, 'absent')+')' for name in sorted(requested - eligible)))
        chosen = requested or eligible
        if not chosen and not args.allow_empty:
            raise ValueError('no fully closed resolved tables to transform')
        with tempfile.TemporaryDirectory(prefix='ppc-static-') as temporary:
            work = Path(temporary)
            modules = []
            for i, path in enumerate(paths):
                bc = work / (str(i)+'.bc')
                run(args.llvm_as, path, '-o', bc)
                modules.append(bc)
            linked = work / 'linked.ll'
            run(args.llvm_link, '-S', *modules, '-o', linked)
            run(args.llvm_opt, '-passes=verify', '-disable-output', linked)
            report = dispatch.collect([linked], args.max_cells)
            families = [f for f in report['functions'] if f['name'] in chosen]
            if len(families) != len(chosen) or any(f['status'] != 'resolved' for f in families):
                raise ValueError('linked registry no longer has a unique complete solution')
            text, audit = transform(linked.read_text(), linked, families, args.entry)
            manifest = dict(schema='ppchibicc.closed-program.v1', entry=args.entry,
                            target=TARGET, datalayout=LAYOUT, inputs=inputs,
                            selected_families=sorted(chosen))
            manifest_sha = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
            ident = 1+max(map(int, re.findall(r'^!(\d+) =', text, re.M)), default=-1)
            text += '\n!ppchibicc.closed_program = !{!'+str(ident)+'}\n!'+str(ident)+' = !{!"ppchibicc.closed-program.v1", !"'+manifest_sha+'"}\n'
            transformed = work / 'static.ll'
            transformed.write_text(text)
            run(args.llvm_as, transformed, '-o', work / 'static.bc')
            run(args.llvm_opt, '-passes=verify', '-disable-output', work / 'static.bc')
            result = dict(manifest, optimization_applied=bool(families), program_completeness='explicit-input-set',
                          manifest_sha256=manifest_sha, output_sha256=hashlib.sha256(text.encode()).hexdigest(),
                          llvm_major=versions[0], functions=families, audit=audit,
                          unselected_families=[dict(name=f['name'], status=f['status'], issues=f['issues'])
                              for f in raw_report['functions'] if f['name'] not in chosen])
            data = json.dumps(result, ensure_ascii=False, indent=2)+'\n'
            atomic_outputs([(args.output, text), (report_path, data)])
        if not args.quiet:
            print('Static PPC dispatch: '+(', '.join(sorted(chosen)) or 'no eligible tables')+'; '+str(len(audit['removed_runtime']))+' runtime functions removed')
        return 0
    except (OSError, UnicodeError, ValueError) as error:
        print('PPC static dispatch: '+str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
