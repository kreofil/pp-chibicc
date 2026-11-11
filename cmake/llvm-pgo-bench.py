#!/usr/bin/env python3
"""Repeat timed, checked PPC kernels; Python and process startup are not timed."""
import csv
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys
import time

VARIANTS = ("native", "O2", "PGO")
WORKLOADS = ("rectangle", "square", "mixed", "growing")
BATCH = 4096
WARMUP = 8
MAX_ROUNDS = 1000000


def settings():
    result = {}
    for name, default, convert, low, high in (
        ("SAMPLES", "7", int, 3, 101),
        ("SECONDS", "0.1", float, 0.01, 1.0),
        ("SEED", "20261111", int, 0, 2**32 - 1),
    ):
        key = "PPCHIBICC_BENCH_" + name
        value = convert(os.environ.get(key, default))
        if not low <= value <= high:
            raise ValueError("{} must be in [{}; {}]".format(key, low, high))
        result[name.lower()] = value
    result["cpu"] = os.environ.get("PPCHIBICC_BENCH_CPU", "auto")
    if result["cpu"] not in ("auto", "none") and int(result["cpu"]) < 0:
        raise ValueError("PPCHIBICC_BENCH_CPU must be auto, none or a CPU number")
    return result


def affinity(requested):
    if not hasattr(os, "sched_getaffinity"):
        if requested not in ("auto", "none"):
            raise ValueError("CPU affinity is unavailable on this platform")
        return {"requested": requested, "effective": "unavailable"}
    allowed = sorted(os.sched_getaffinity(0))
    if requested != "none":
        selected = allowed[0] if requested == "auto" else int(requested)
        if selected not in allowed:
            raise ValueError("CPU {} is outside allowed affinity {}".format(selected, allowed))
        os.sched_setaffinity(0, {selected})
    return {"requested": requested, "allowed": allowed,
            "effective": sorted(os.sched_getaffinity(0))}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


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


def summarize(samples):
    result = {}
    for mode, variant in itertools.product(range(4), VARIANTS):
        group = [s for s in samples if s["mode"] == mode and s["variant"] == variant]
        wall = [s["wall_seconds"] * 1e9 / s["calls"] for s in group]
        median = statistics.median(wall)
        mad = statistics.median(abs(v - median) for v in wall)
        result[mode, variant] = {
            "workload": WORKLOADS[mode], "variant": variant, "rounds": group[0]["rounds"],
            "median_ns": median, "mad_percent": mad / median * 100,
            "min_ns": min(wall), "max_ns": max(wall),
            "median_cpu_ns": statistics.median(s["cpu_seconds"] * 1e9 / s["calls"] for s in group),
            "median_cpu_wall": statistics.median(s["cpu_seconds"] / s["wall_seconds"] for s in group),
        }
    return result


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
    rng = random.Random(config["seed"])
    info["calibration"], rounds_by_mode = [], {}
    for mode in range(4):
        estimates = []
        variants = list(VARIANTS)
        rng.shuffle(variants)
        for variant in variants:
            rounds = 32
            for _ in range(8):
                sample = execute(directory, variant, mode, rounds)
                info["calibration"].append(sample)
                elapsed = sample["wall_seconds"]
                if elapsed >= min(0.02, config["seconds"] / 4) or rounds == MAX_ROUNDS:
                    break
                rounds = min(MAX_ROUNDS, max(rounds + 1, math.ceil(rounds * 0.025 / elapsed)))
            estimates.append(math.ceil(rounds * config["seconds"] / elapsed))
        # Equal work for all three variants; size it using the fastest one.
        rounds_by_mode[mode] = max(1, min(MAX_ROUNDS, max(estimates)))
        print("{}: {} rounds/sample for all variants".format(WORKLOADS[mode], rounds_by_mode[mode]), flush=True)
    info["rounds_by_mode"] = rounds_by_mode
    samples = []
    for repeat in range(config["samples"]):
        jobs = list(itertools.product(range(4), VARIANTS))
        rng.shuffle(jobs)
        for mode, variant in jobs:
            sample = execute(directory, variant, mode, rounds_by_mode[mode])
            sample.update(repeat=repeat + 1, order=len(samples) + 1)
            samples.append(sample)
            # Retain each finished sample even if a later process is interrupted.
            (directory / "samples.json").write_text(
                json.dumps(dict(info, samples=samples), indent=2) + "\n", encoding="utf-8")
        print("Completed measurement block {}/{}".format(repeat + 1, config["samples"]), flush=True)
    for variant, checksum in info["executable_sha256"].items():
        if digest(directory / "bin" / (variant + ".exe")) != checksum:
            raise RuntimeError("executable changed during measurement: " + variant)

    with (directory / "samples.tsv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(samples[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(samples)
    summary = summarize(samples)
    with (directory / "summary.tsv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(next(iter(summary.values()))), delimiter="\t")
        writer.writeheader()
        writer.writerows(summary.values())
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
