#!/usr/bin/env python3
"""Held-out collection workloads using the project's real ppVector library."""
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
import time

from llvm_bench import affinity, digest, measure, settings, summarize, write_tables

VARIANTS = ("native", "O2", "PGO90", "PGO50")
WORKLOADS = ("rect90", "balanced", "trian90", "large-balanced")
WARMUP = 2
MAX_ROUNDS = 10000


def dataset(directory, name, rows, seed=None):
    path = directory / "data" / (name + ".txt")
    path.write_text(str(len(rows)) + "\n" + "".join("{} {} {} {}\n".format(*r) for r in rows))
    perimeters = sorted(2 * (a + b) if kind == 1 else a + b + c for kind, a, b, c in rows)
    return {"name": name, "size": len(rows), "seed": seed,
            "rectangles": sum(r[0] == 1 for r in rows), "sha256": digest(path),
            "per_round_checksum": sum(p * (i + 1) for i, p in enumerate(perimeters))}


def generated(directory, name, size, percent, seed):
    state, rows = seed, []

    def next_number():
        nonlocal state
        state = (1664525 * state + 1013904223) & 0xffffffff
        return state

    for i in range(size):
        if i < size * percent // 100:
            rows.append((1, 1 + next_number() % 200, 1 + next_number() % 200, 0))
        else:
            scale = 1 + next_number() % 100
            rows.append((2, 3 * scale, 4 * scale, 5 * scale))
    for i in range(size - 1, 0, -1):
        j = next_number() % (i + 1)
        rows[i], rows[j] = rows[j], rows[i]
    return dataset(directory, name, rows, seed)


def prepare(directory):
    (directory / "data").mkdir(parents=True, exist_ok=True)
    records = {}
    for name, size, percent, seed in (
        ("train90", 4096, 90, 1), ("train50", 4096, 50, 2),
        ("rect90", 8192, 90, 101), ("balanced", 8192, 50, 102),
        ("trian90", 8192, 10, 103), ("large-balanced", 32768, 50, 104),
        ("small-shuffled", 127, 50, 105),
    ):
        records[name] = generated(directory, name, size, percent, seed)
    for name, rows in (
        ("empty", []), ("one-rect", [(1, 2, 4, 0)]),
        ("one-trian", [(2, 3, 4, 5)]),
        ("ties", [(1, 2, 4, 0), (2, 3, 4, 5)] * 6),
        ("reverse", [(1, i, 1, 0) for i in range(20, 0, -1)]),
    ):
        records[name] = dataset(directory, name, rows)
    invalid = {
        "negative": "-1\n", "too-large": "32769\n", "truncated": "2\n1 2 4 0\n",
        "unknown-kind": "1\n3 2 4 0\n", "triangle": "1\n2 1 1 4\n",
        "side": "1\n1 0 4 0\n", "extra-row": "0\n1 2 4 0\n",
    }
    for name, text in invalid.items():
        (directory / "data" / ("invalid-" + name + ".txt")).write_text(text)
    (directory / "datasets.json").write_text(json.dumps(records, indent=2) + "\n")


def validate(result, record, rounds):
    per_round = record["per_round_checksum"]
    expected = {"size": record["size"], "rounds": rounds, "items": record["size"] * rounds,
                "warmup_rounds": WARMUP, "checksum": per_round * rounds,
                "per_round_checksum": per_round, "warmup_checksum": per_round * WARMUP}
    if any(result.get(k) != v for k, v in expected.items()):
        raise RuntimeError("incorrect collection result: " + str(result))
    for field in ("wall_seconds", "cpu_seconds"):
        if not math.isfinite(result[field]) or result[field] <= 0:
            raise RuntimeError("invalid timer result: " + str(result))


def execute(directory, records, variant, name, rounds):
    command = [str(directory / "bin" / (variant + ".exe")),
               str(directory / "data" / (name + ".txt")), str(rounds)]
    process = subprocess.run(command, text=True, capture_output=True, timeout=120)
    if process.returncode or process.stderr:
        raise RuntimeError("{} failed ({}): {}".format(command, process.returncode, process.stderr))
    result = json.loads(process.stdout)
    validate(result, records[name], rounds)
    result.update(variant=variant, workload=name, input_sha256=records[name]["sha256"])
    return result


def check(directory, records):
    results = []
    for variant in VARIANTS:
        for name in ("empty", "one-rect", "one-trian", "ties", "reverse", "small-shuffled"):
            for rounds in (1, 3):
                results.append(execute(directory, records, variant, name, rounds))
        executable = str(directory / "bin" / (variant + ".exe"))
        invalid_args = [[str(p), "1"] for p in sorted((directory / "data").glob("invalid-*.txt"))]
        valid_path = str(directory / "data" / "one-rect.txt")
        invalid_args += [[valid_path, r] for r in ("0", "-1", "10001", "bad")]
        invalid_args += [[valid_path], [valid_path, "1", "extra"], [valid_path + ".missing", "1"]]
        for args in invalid_args:
            process = subprocess.run([executable] + args, text=True, capture_output=True, timeout=10)
            if process.returncode != 2 or process.stdout or process.stderr:
                raise RuntimeError("invalid-input check failed: " + str(args))
    return results


def main():
    config = settings()
    if sys.argv[1:] == ["--check-config"]:
        return
    if len(sys.argv) == 3 and sys.argv[1] == "--prepare":
        prepare(Path(sys.argv[2]).resolve())
        return
    if len(sys.argv) != 2:
        raise ValueError("expected benchmark result directory")
    directory = Path(sys.argv[1]).resolve()
    (directory / "report.md").unlink(missing_ok=True)
    records = json.loads((directory / "datasets.json").read_text())
    for name, record in records.items():
        if digest(directory / "data" / (name + ".txt")) != record["sha256"]:
            raise RuntimeError("dataset changed: " + name)
    for name in ("train90", "train50"):
        validate(json.loads((directory / "training" / (name + ".stdout")).read_text()), records[name], 2)
    info = {"utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "platform": platform.platform(), "python": sys.version, "settings": config,
            "affinity": affinity(config["cpu"]), "datasets": records, "warmup_rounds": WARMUP,
            "executable_sha256": {v: digest(directory / "bin" / (v + ".exe")) for v in VARIANTS}}
    print("Checking collection sorting against independent perimeters", flush=True)
    info["checks"] = check(directory, records)

    def run(variant, mode, rounds):
        sample = execute(directory, records, variant, WORKLOADS[mode], rounds)
        sample["mode"] = mode
        return sample

    samples = measure(directory, info, VARIANTS, WORKLOADS, run, MAX_ROUNDS)
    for name, record in records.items():
        if digest(directory / "data" / (name + ".txt")) != record["sha256"]:
            raise RuntimeError("dataset changed during measurement: " + name)
    summary = summarize(samples, VARIANTS, WORKLOADS, units="items")
    write_tables(directory, samples, summary)
    report = ["# Коллекция фигур: состав обучения и время сортировки", "",
              "Дата UTC: {}. Повторов: {}; цель калибровки: {} с; seed порядка: {}; affinity: {}.".format(
                  info["utc"], config["samples"], config["seconds"], config["seed"], info["affinity"]["effective"]),
              "Проверки исходного примера, отсортированности и контрольных сумм пройдены.",
              "PGO90 обучен на 90% прямоугольников, PGO50 — на 50%; "
              "все измеряемые данные имеют другие seeds и размеры.", "",
              "Медиана мс на полный проход: восстановление вектора, сортировка и обработка результата. "
              "В скобках — MAD в % от медианы; отношения больше 1 означают ускорение.", "",
              "| Набор | Фигур | native | O2 | PGO90 | PGO50 | O2/PGO90 | O2/PGO50 |",
              "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for mode, name in enumerate(WORKLOADS):
        count = records[name]["size"]
        cells = ["{:.3f} ({:.1f}%)".format(summary[mode, v]["median_ns"] * count / 1e6,
                                         summary[mode, v]["mad_percent"]) for v in VARIANTS]
        ratios = [summary[mode, "O2"]["median_ns"] / summary[mode, v]["median_ns"] for v in VARIANTS[2:]]
        report.append("| {} | {} | {} | {:.3f} | {:.3f} |".format(name, count, " | ".join(cells), *ratios))
    report += ["", "native использует исходный backend без оптимизирующего pipeline. "
               "Для выделения эффекта профиля сравниваются O2 и два PGO-варианта.", "",
               "Во всех вариантах один C-таймер, одна libc/qsort и одинаковые входные файлы. "
               "Ввод, выделение памяти, конструкторы, эталонная сортировка и два прогрева "
               "находятся вне интервала. Перед каждым проходом восстанавливается исходный порядок; "
               "копирование, qsort и проход с проверкой порядка входят в измерение.", "",
               "Контрольная сумма учитывает позицию каждого периметра в отсортированном векторе. "
               "Он проверен независимыми вычислениями в C и Python; значения целые, без погрешности double.", ""]
    noisy = sum(row["mad_percent"] > 10 for row in summary.values())
    preempted = sum(s["cpu_seconds"] / s["wall_seconds"] < 0.8 for s in samples)
    if noisy or preempted:
        report += ["Рядов с MAD > 10%: {}; замеров с CPU/wall < 0.8: {}.".format(noisy, preempted), ""]
    report += ["Состав объектов не равен составу косвенных вызовов: qsort вызывает компаратор "
               "в зависимости от данных. Фактические частоты: training/PGO90/profile.txt и "
               "training/PGO50/profile.txt. Продвижения: PGO90/promotion.yaml, PGO50/promotion.yaml.", "",
               "Это вычислительная часть примера ppvector-figures с воспроизводимыми данными; "
               "время чтения и печати программы не измеряется. Результат зависит от машины и набора.", "",
               "Сырые времена и порядок: samples.json, samples.tsv. Статистика в нс/элемент: summary.tsv. "
               "Входы и эталоны: data/, datasets.json. Исходники, профили, pipeline, IR, версии "
               "и хеши: source/, training/, O2/, PGO90/, PGO50/, manifest.txt.", ""]
    (directory / "report.md").write_text("\n".join(report), encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError, KeyError, subprocess.TimeoutExpired) as error:
        sys.exit("LLVM figures benchmark: " + str(error))
