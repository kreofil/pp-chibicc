#!/usr/bin/env python3
"""Inspect ppchibicc.dispatch.v1 named metadata in textual LLVM modules.

This is an analysis tool, not an LLVM optimizer or a proof that the supplied
modules form a whole program. No source text or metadata is executed.
"""
from __future__ import annotations

import argparse
import itertools
import json
import math
from pathlib import Path
import re
import sys


SCHEMA = "ppchibicc.dispatch.v1"
STRING = r'"((?:\\[0-9A-Fa-f]{2}|[^"\\])*)"'
FIELD = re.compile(
    r'\s*(?:!'+STRING+r'|ptr\s+@(?:'+STRING+r'|([-a-zA-Z$._0-9]+))'
    r'|ptr\s+(null)|i(1|32)\s+(true|false|-?\d+))\s*(,|$)'
)


def unescape(value):
    raw = bytearray()
    pos = 0
    while pos < len(value):
        if value[pos] == "\\":
            raw.append(int(value[pos + 1:pos + 3], 16))
            pos += 3
        else:
            raw.extend(value[pos].encode("utf-8"))
            pos += 1
    return raw.decode("utf-8")


def fields(text):
    result = []
    pos = 0
    while pos < len(text):
        match = FIELD.match(text, pos)
        if not match:
            raise ValueError("unsupported dispatch metadata operand: " + text[pos:pos+80])
        string, quoted, symbol, null, width, number, comma = match.groups()
        if string is not None:
            result.append(("string", unescape(string)))
        elif quoted is not None or symbol is not None or null is not None:
            result.append(("ptr", unescape(quoted) if quoted is not None else symbol))
        else:
            result.append(("i" + width, {"true": 1, "false": 0}.get(number, number)))
            result[-1] = (result[-1][0], int(result[-1][1]))
        pos = match.end()
        if comma and pos == len(text):
            raise ValueError("trailing comma in dispatch metadata")
    return result


def values(record, types):
    if [kind for kind, _ in record] != types.split():
        raise ValueError("invalid dispatch metadata record shape")
    return [value for _, value in record]


def read_module(path):
    text = path.read_text(encoding="utf-8")
    definitions = {unescape(quoted) if quoted else symbol
                   for quoted, symbol in re.findall(
                       r'^define\b[^\n]*?@(?:'+STRING+r'|([-a-zA-Z$._0-9]+))\s*\(', text, re.M)}
    root = re.findall(r'^!ppchibicc\.dispatch\s*=\s*!\{([^\n]*)\}\s*$', text, re.M)
    if len(root) != 1:
        raise ValueError(f"{path}: expected one !ppchibicc.dispatch root")
    ids = re.findall(r'!(\d+)', root[0])
    if not ids or re.sub(r'!\d+|[\s,]', '', root[0]):
        raise ValueError(f"{path}: invalid dispatch metadata root")
    if len(ids) != len(set(ids)):
        raise ValueError(f"{path}: repeated dispatch metadata record")
    nodes = dict(re.findall(r'^!(\d+)\s*=\s*!\{([^\n]*)\}\s*$', text, re.M))
    records = []
    for ident in ids:
        if ident not in nodes:
            raise ValueError(f"{path}: missing metadata node !{ident}")
        record = fields(nodes[ident])
        if len(record) < 3 or record[0] != ("string", SCHEMA):
            raise ValueError(f"{path}: unsupported dispatch metadata schema")
        records.append(record)
    units = set()
    families = {}
    for record in records:
        if record[1] == ("string", "unit"):
            _, _, source = values(record, "string string string")
            if source in units:
                raise ValueError(f"{path}: repeated unit {source}")
            units.add(source)
    if not units:
        raise ValueError(f"{path}: no dispatch unit marker")
    for record in records:
        kind = record[1][1]
        source = record[2][1]
        if source not in units:
            raise ValueError(f"{path}: record without unit marker")
        if kind == "unit":
            continue
        if len(record) < 4 or record[3][0] != "string":
            raise ValueError(f"{path}: missing family name")
        name = record[3][1]
        family = families.setdefault((source, name), dict(
            source=source, name=name, axes={}, handlers=[], signature=None,
            table=None, excluded=False, arity=None))
        if kind == "excluded":
            values(record, "string string string string string")
            if record[4][1] != "open-or-opaque" or family["excluded"]:
                raise ValueError(f"{path}: invalid exclusion for {name}")
            family["excluded"] = True
        elif kind == "family":
            _, _, _, _, signature, arity, table = values(
                record, "string string string string string i32 ptr")
            if family["signature"] is not None or arity <= 0:
                raise ValueError(f"{path}: invalid family {name}")
            family.update(signature=signature, arity=arity, table=table)
        elif kind == "axis":
            _, _, _, _, index, type_name, form, extent, size, align, labels = values(
                record, "string string string string i32 string string i32 i32 i32 string")
            alternatives = labels.split(";")
            if (index < 0 or index in family["axes"] or extent < 1 or size < 0 or align < 1
                    or form not in ("case", "enum", "nullptr", "union")
                    or len(alternatives) != extent
                    or any(not label.startswith(str(i)+":") or not label.split(":", 1)[1]
                           for i, label in enumerate(alternatives))):
                raise ValueError(f"{path}: invalid closed axis for {name}")
            family["axes"][index] = dict(type=type_name, form=form, extent=extent,
                size=size, align=align, alternatives=[label.split(":", 1)[1] for label in alternatives])
        elif kind == "handler":
            _, _, _, _, coordinate, symbol, definition, private = values(
                record, "string string string string string ptr i1 i1")
            if definition not in (0, 1) or private not in (0, 1):
                raise ValueError(f"{path}: invalid handler flags")
            coords = None if coordinate == "*" else tuple(int(v) for v in coordinate.split(","))
            family["handlers"].append(dict(source=source, symbol=symbol,
                definition=bool(definition), ir_definition=symbol in definitions,
                private=bool(private), coordinates=coords))
        else:
            raise ValueError(f"{path}: unknown dispatch record {kind}")
    for family in families.values():
        if family["excluded"]:
            if family["signature"] is not None or family["axes"] or family["handlers"]:
                raise ValueError(f"{path}: exclusion mixed with closed data")
            continue
        arity = family["arity"]
        if arity is None or set(family["axes"]) != set(range(arity)):
            raise ValueError(f"{path}: incomplete axes for {family['name']}")
        family["axes"] = [family["axes"][i] for i in range(arity)]
        for handler in family["handlers"]:
            coords = handler["coordinates"]
            if coords is not None and (len(coords) != arity or any(
                    c < 0 or c >= axis["extent"] for c, axis in zip(coords, family["axes"]))):
                raise ValueError(f"{path}: invalid handler coordinates")
    return units, list(families.values())


def handler_key(handler):
    return (handler["source"] if handler["private"] else "", handler["symbol"] or "")


def collect(paths, max_cells):
    by_name = {}
    units = {}
    for path in sorted(paths, key=str):
        module_units, families = read_module(path)
        for unit in sorted(module_units):
            if unit in units:
                raise ValueError(f"unit {unit} is present in more than one input")
            units[unit] = str(path)
        for family in families:
            by_name.setdefault(family["name"], []).append(family)
    result = []
    for name, entries in sorted(by_name.items()):
        closed = [entry for entry in entries if not entry["excluded"]]
        blocked = sorted(entry["source"] for entry in entries if entry["excluded"])
        out = dict(name=name, status="excluded", issues=[], axes=[], cells=[],
                   excluded_units=blocked, registrations=[], cell_count=0)
        result.append(out)
        if not closed:
            continue
        reference = closed[0]
        out.update(axes=reference["axes"], signature=reference["signature"], table=reference["table"])
        if any((entry["axes"], entry["signature"], entry["table"]) !=
               (reference["axes"], reference["signature"], reference["table"]) for entry in closed):
            out.update(status="inconsistent", issues=["closed type descriptions, ABI or table symbols disagree"])
            continue
        handlers = [h for entry in closed for h in entry["handlers"]]
        out["registrations"] = sorted(handlers, key=lambda h: (h["source"], h["symbol"] or "", str(h["coordinates"])))
        definitions = {}
        for handler in handlers:
            if handler["definition"] and handler["ir_definition"]:
                definitions.setdefault(handler_key(handler), []).append(handler)
        if blocked:
            out["issues"].append("participating units have open/opaque procedural types")
        if any(entry["table"] is None for entry in closed):
            out["issues"].append("dispatch table reference was removed")
        if any(len(items) > 1 for items in definitions.values()):
            out["issues"].append("multiple definitions for the same handler symbol")
        if any(h["symbol"] is None or handler_key(h) not in definitions for h in handlers):
            out["issues"].append("handler definitions are absent from the supplied metadata")
        # A declaration with the same linker identity must describe the same
        # tuple. Otherwise constructor registration cannot be reconstructed.
        identities = {}
        for h in handlers:
            identities.setdefault(handler_key(h), set()).add(h["coordinates"])
        if any(len(coords) > 1 for coords in identities.values()):
            out["issues"].append("handler symbol is associated with different coordinates")
        extents = [axis["extent"] for axis in out["axes"]]
        out["cell_count"] = math.prod(extents)
        if out["cell_count"] > max_cells:
            out["status"] = "limited"
            out["issues"].append(f"matrix exceeds report limit {max_cells}")
            continue
        defaults = {handler_key(h): h for h in handlers if h["coordinates"] is None}
        concrete = {}
        for h in handlers:
            if h["coordinates"] is not None:
                concrete.setdefault(h["coordinates"], {})[handler_key(h)] = h
        for index, coords in enumerate(itertools.product(*(range(n) for n in extents))):
            candidates = concrete.get(coords) or defaults
            state = "missing" if not candidates else "order-dependent" if len(candidates) > 1 else "resolved"
            if any(not h["symbol"] or handler_key(h) not in definitions for h in candidates.values()):
                state = "unresolved"
            out["cells"].append(dict(index=index, coordinates=coords,
                alternatives=[axis["type"]+"."+axis["alternatives"][i] for i, axis in zip(coords, out["axes"])],
                kind="concrete" if coords in concrete else "default" if defaults else "missing",
                state=state, handlers=[dict(symbol=h["symbol"], private=h["private"],
                    source=h["source"] if h["private"] else None,
                    definitions=sorted(d["source"] for d in definitions.get(key, [])))
                    for key, h in sorted(candidates.items())]))
        states = {cell["state"] for cell in out["cells"]}
        out["status"] = ("blocked" if out["issues"] else "incomplete" if "missing" in states
                         else "unresolved" if "unresolved" in states
                         else "order-dependent" if "order-dependent" in states else "resolved")
    return dict(schema="ppchibicc.closed-dispatch.report.v1", optimization_applied=False,
                program_completeness="not-established", units=[dict(source=s, input=p) for s, p in sorted(units.items())],
                functions=result)


def md(value):
    return str(value).replace("\\", "\\\\").replace("|", "\\|").replace("\n", " ").replace("\r", " ")


def markdown(report):
    lines = ["# Матрицы закрытой диспетчеризации 4П", "",
        "Отчёт описывает только перечисленные LLVM-модули. Полнота всей программы не установлена; оптимизация не выполнялась.", "",
        "`resolved` означает, что для каждой ячейки среди переданных метаданных найден единственный обработчик с определением.",
        "`order-dependent` означает несколько разных регистраций для одной ячейки; текущая runtime-семантика зависит от порядка регистрации.", "",
        "| Функция | Размер | Состояние |", "| --- | ---: | --- |"]
    for fn in report["functions"]:
        lines.append(f"| {md(fn['name'])} | {fn['cell_count']} | {fn['status']} |")
    for fn in report["functions"]:
        lines += ["", "## " + md(fn["name"]), "", "Состояние: `"+fn["status"]+"`."]
        if fn["axes"]:
            lines += ["", "Измерения: " + " × ".join(str(axis["extent"]) for axis in fn["axes"]) + "."]
        for issue in fn["issues"]:
            lines += ["", md(issue) + "."]
        if fn["excluded_units"]:
            lines += ["", "Открытые/непрозрачные объявления: " + ", ".join(md(s) for s in fn["excluded_units"]) + "."]
        if not fn["cells"]:
            continue
        lines += ["", "| Индекс | Комбинация | Обработчик | Источник | Состояние |", "| ---: | --- | --- | --- | --- |"]
        for cell in fn["cells"]:
            targets = "; ".join((h["source"]+"::" if h["private"] else "") + (h["symbol"] or "null") for h in cell["handlers"])
            lines.append(f"| {cell['index']} | {md(', '.join(cell['alternatives']))} | {md(targets or '—')} | {cell['kind']} | {cell['state']} |")
    return "\n".join(lines)+"\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("modules", nargs="+", type=Path, help="textual LLVM IR, raw or linked")
    parser.add_argument("--json", type=Path, help="write machine-readable report")
    parser.add_argument("--markdown", type=Path, help="write readable matrix report")
    parser.add_argument("--max-cells", type=int, default=4096)
    parser.add_argument("--require-resolved", action="store_true", help="fail if any included closed family is not resolved")
    args = parser.parse_args()
    try:
        if args.max_cells < 1:
            raise ValueError("--max-cells must be positive")
        if len(set(p.resolve() for p in args.modules)) != len(args.modules):
            raise ValueError("duplicate input module")
        outputs = [p.resolve() for p in (args.json, args.markdown) if p is not None]
        if len(outputs) != len(set(outputs)) or set(outputs) & {p.resolve() for p in args.modules}:
            raise ValueError("report outputs must be distinct from each other and the inputs")
        report = collect(args.modules, args.max_cells)
        if args.json:
            args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
        if args.markdown:
            args.markdown.write_text(markdown(report), encoding="utf-8")
        if not args.json and not args.markdown:
            print(markdown(report), end="")
        if args.require_resolved and any(fn["axes"] and fn["status"] != "resolved" for fn in report["functions"]):
            return 1
        return 0
    except (OSError, UnicodeError, ValueError) as error:
        print(f"PPC dispatch report: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
