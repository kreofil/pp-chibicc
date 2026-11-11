#!/usr/bin/env python3
"""Repeat timed, checked PPC kernels; Python and process startup are not timed."""
import itertools
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
import time

from llvm_bench import affinity, digest, measure, settings, summarize, write_tables

VARIANTS = ("native", "O2", "PGO")
WORKLOADS = ("rectangle", "square", "mixed", "growing")
BATCH = 4096
WARMUP = 8
MAX_ROUNDS = 1000000


def execute(directory, variant, mode, rounds):
    command = [str(directory / "bin" / (variant + ".exe")), str(mode), str(rounds)]
    process = subprocess.run(command, text=True, capture_output=True, timeout=120)
    if process.returncode or process.stderr:
        raise RuntimeError("{} failed ({}): {}".format(command, process.returncode, process.stderr))
    result = json.loads(process.stdout)
    if (result["mode"], result["rounds"], result["calls"], result["warmup_rounds"]) != (
        mode, rounds, rounds * BATCH, WARMUP
    ):
        raise RuntimeError("incorrect timer metadata: " + process.stdout)
    for field in ("wall_seconds", "cpu_seconds"):
        if not math.isfinite(result[field]) or result[field] <= 0:
            raise RuntimeError("invalid duration: " + process.stdout)
    result.update(variant=variant, workload=WORKLOADS[mode])
    return result


def reference(mode, rounds):
    # Small independent simulation for checks, rather than the timer's formula.
    growing, visits, warmup, checksum = 2.0, 0, 0.0, 0.0
    for i in range((WARMUP + rounds) * BATCH):
        if mode == 3 and i % 2 == 0:
            growing += 0.5
            visits += 1
            value = growing
        else:
            value = 9.0 if mode == 1 or (mode == 2 and i % 2) else 10.0
        if i < WARMUP * BATCH:
            warmup += value
        else:
            checksum += value
    return warmup, checksum, visits


def check(directory):
    results = []
    for variant, mode, rounds in itertools.product(VARIANTS, range(4), (1, 3)):
        result = execute(directory, variant, mode, rounds)
        actual = (result["warmup_checksum"], result["checksum"], result["visits"])
        if actual != reference(mode, rounds):
            raise RuntimeError("reference mismatch: " + str(result))
        results.append(result)
    for variant in VARIANTS:
        for args in (("-1", "1"), ("4", "1"), ("0", "0"), ("0", "-1"),
                     ("0", "1000001"), ("0", "bad"), ("bad", "1"), ("0", "1", "extra")):
            command = [str(directory / "bin" / (variant + ".exe"))] + list(args)
            result = subprocess.run(command, text=True, capture_output=True, timeout=10)
            if result.returncode != 2 or result.stdout or not result.stderr.startswith("usage:"):
                raise RuntimeError("invalid argument check failed: " + str(command))
    return results


def main():
    config = settings()
    if sys.argv[1:] == ["--check-config"]:
        return
    if len(sys.argv) != 2:
        raise ValueError("expected benchmark result directory")
    directory = Path(sys.argv[1]).resolve()
    # A failed repeat must not leave a previous report looking current.
    (directory / "report.md").unlink(missing_ok=True)
    info = {
        "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "platform": platform.platform(), "python": sys.version,
        "affinity": affinity(config["cpu"]), "settings": config,
        "batch_calls": BATCH, "warmup_rounds": WARMUP,
        "executable_sha256": {v: digest(directory / "bin" / (v + ".exe")) for v in VARIANTS},
    }
    print("Checking benchmark results and argument handling", flush=True)
    info["checks"] = check(directory)
    samples = measure(directory, info, VARIANTS, WORKLOADS,
                      lambda variant, mode, rounds: execute(directory, variant, mode, rounds),
                      MAX_ROUNDS)
    summary = summarize(samples, VARIANTS, WORKLOADS)
    write_tables(directory, samples, summary)
    report = [
        "# Время диспетчеризации 4П: native, O2 и PGO", "",
        "Дата UTC: {}. Профиль: только rectangle, 4096 вызовов.".format(info["utc"]),
        "Проверки результатов пройдены. Обучение и измерения используют разные исполняемые файлы.",
        "Повторов: {}; цель калибровки: {} с; seed: {}; affinity: {}.".format(
            config["samples"], config["seconds"], config["seed"], info["affinity"]["effective"]), "",
        "В ячейках — медиана нс/вызов (MAD в % от медианы), по CLOCK_MONOTONIC.",
        "O2/PGO > 1 означает ускорение PGO; < 1 — замедление на этой нагрузке.", "",
        "| Нагрузка | native | O2 | PGO | O2/PGO |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for mode, workload in enumerate(WORKLOADS):
        cells = ["{:.3f} ({:.1f}%)".format(summary[mode, v]["median_ns"], summary[mode, v]["mad_percent"])
                 for v in VARIANTS]
        ratio = summary[mode, "O2"]["median_ns"] / summary[mode, "PGO"]["median_ns"]
        report.append("| {} | {} | {:.3f} |".format(workload, " | ".join(cells), ratio))
    report += ["", "native — исходный backend ppchibicc без оптимизирующего pipeline. "
               "Эффект профиля оценивается сравнением O2 и PGO.", "",
               "Каждый процесс сначала выполняет 8 прогревочных пакетов по 4096 вызовов. "
               "На одной нагрузке число измеряемых вызовов одинаково для всех вариантов. "
               "Порядок запусков перемешивается в каждом блоке повторов.", "",
               "Время включает цикл вызовов dispatch_many; создание процесса, конструкторы, "
               "подготовка объектов, прогрев и печать результата находятся вне интервала.", "",
               "rectangle — обученный обработчик; square и mixed используют отсутствовавший "
               "в профиле квадрат. growing — новая специализация из отдельного объекта, "
               "меняющая visits и payload; она чередуется с rectangle.", ""]
    noisy = sum(row["mad_percent"] > 10 for row in summary.values())
    preempted = sum(s["cpu_seconds"] / s["wall_seconds"] < 0.8 for s in samples)
    if noisy:
        report += ["В {} рядах MAD превышает 10%: повторите измерение на менее занятой машине.".format(noisy), ""]
    if preempted:
        report += ["В {} замерах CPU/wall < 0.8: в интервал вошли заметные ожидания планировщика.".format(preempted), ""]
    report += ["Это микробенчмарк двух чередующихся объектов и коротких обработчиков. "
               "Он не предсказывает ускорение прикладной программы; MAD описывает разброс, "
               "а не доверительный интервал.", "",
               "Сырые замеры, калибровка, seed, порядок и контрольные суммы: samples.json. "
               "Таблицы: samples.tsv и summary.tsv (включая min/max и CPU time). "
               "Версии и параметры: manifest.txt и demo/manifest.txt. "
               "Профиль и IR: demo/training и demo/PGO.", ""]
    (directory / "report.md").write_text("\n".join(report), encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError, KeyError, subprocess.TimeoutExpired) as error:
        sys.exit("LLVM PGO benchmark: " + str(error))
