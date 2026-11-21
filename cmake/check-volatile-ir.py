#!/usr/bin/env python3
"""Check observable volatile accesses in raw and optimized LLVM IR."""
import re
import sys
from pathlib import Path


def check(path):
    text = Path(path).read_text()
    functions = dict(re.findall(
        r'^define [^\n]+ @"?([A-Za-z_][A-Za-z_0-9]*)"?\([^\n]*\)[^\n]*\{\n(.*?)^}',
        text, re.M | re.S))

    def count(name, operation, typ=None):
        pattern = r'\b' + operation + r' volatile ' + (re.escape(typ) + r'\b' if typ else '')
        return len(re.findall(pattern, functions[name]))

    def expect(name, loads, stores, typ=None):
        actual = (count(name, 'load', typ), count(name, 'store', typ))
        assert actual == (loads, stores), (path, name, typ, actual, (loads, stores))

    for name in ['volatile_sum', 'volatile_member', 'volatile_discard']:
        expect(name, 2, 0, 'i32')
    expect('volatile_write', 0, 1)
    expect('volatile_local_init', 1, 1)
    expect('volatile_typeof_lvalue', 1, 1)
    for name in ['volatile_typeof_value', 'volatile_typeof_assignment', 'volatile_typeof_cast']:
        expect(name, 0, 0)
    assert '@llvm.memset' not in functions['volatile_local_init'], (path, 'redundant zero initialization')
    expect('volatile_update', 1, 1)
    expect('volatile_post', 1, 1)
    for name in ['volatile_address', 'plain_member', 'plain_copy']:
        expect(name, 0, 0)
    for name in ['pointer_to_volatile', 'volatile_conditional', 'volatile_matrix',
                 'volatile_vla', 'volatile_global_matrix']:
        expect(name, 1, 0, 'i32')
    for name in ['volatile_pointer', 'volatile_bound']:
        expect(name, 2, 1, 'ptr')
        expect(name, 0, 0, 'i32')
    expect('volatile_both', 2, 1, 'ptr')
    expect('volatile_both', 2, 0, 'i32')
    expect('volatile_member_pointer', 1, 0, 'ptr')
    expect('volatile_member_pointer', 0, 0, 'i32')
    expect('volatile_discard_pair', 2, 0)
    expect('volatile_discard_comma', 1, 0)
    expect('volatile_discard_stmt', 1, 0)
    assert count('volatile_loop_step', 'load') >= 1, (path, 'loop step read removed')
    expect('volatile_forward', 1, 0, 'i32')
    expect('volatile_forward_union', 1, 0, 'i32')
    expect('volatile_bitfield_read', 1, 0)
    expect('volatile_bitfield_write', 1, 1)
    expect('pair_return', 1, 0)
    assert count('volatile_discard_cond', 'load') >= 1, (path, 'discarded conditional')
    assert count('volatile_poll', 'load', 'i32') >= 2, (path, 'polling load removed')
    operations = re.findall(r'\b(load|store) volatile ', functions['volatile_sequence'])
    assert operations == ['store', 'store', 'load', 'store'], (path, operations)
    for name in ['volatile_copy_to', 'volatile_copy_from', 'big_return']:
        body = functions[name]
        assert (re.search(r'@llvm\.mem(?:move|cpy)[^\n]+i1 true', body)
                or count(name, 'load') or count(name, 'store')), (path, name)
    print('Volatile IR checks passed:', path)


for path in sys.argv[1:]:
    check(path)
