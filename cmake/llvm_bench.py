"""Shared measurement protocol for the optional LLVM benchmark targets."""
import csv
import hashlib
import itertools
import json
import math
import os
import random
import statistics


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


def measure(directory, info, variants, workloads, execute, max_rounds):
    config = info["settings"]
    rng = random.Random(config["seed"])
    info["calibration"], rounds_by_mode = [], {}
    for mode, workload in enumerate(workloads):
        estimates = []
        order = list(variants)
        rng.shuffle(order)
        for variant in order:
            rounds = min(32, max_rounds)
            for _ in range(8):
                sample = execute(variant, mode, rounds)
                info["calibration"].append(sample)
                elapsed = sample["wall_seconds"]
                if elapsed >= min(0.02, config["seconds"] / 4) or rounds == max_rounds:
                    break
                rounds = min(max_rounds, max(rounds + 1, math.ceil(rounds * 0.025 / elapsed)))
            estimates.append(math.ceil(rounds * config["seconds"] / elapsed))
        # Equal work for every variant, sized using the fastest one.
        rounds_by_mode[mode] = max(1, min(max_rounds, max(estimates)))
        print("{}: {} rounds/sample for all variants".format(workload, rounds_by_mode[mode]), flush=True)
    info["rounds_by_mode"] = rounds_by_mode
    samples = []
    for repeat in range(config["samples"]):
        jobs = list(itertools.product(range(len(workloads)), variants))
        rng.shuffle(jobs)
        for mode, variant in jobs:
            sample = execute(variant, mode, rounds_by_mode[mode])
            sample.update(repeat=repeat + 1, order=len(samples) + 1)
            samples.append(sample)
            (directory / "samples.json").write_text(
                json.dumps(dict(info, samples=samples), indent=2) + "\n", encoding="utf-8")
        print("Completed measurement block {}/{}".format(repeat + 1, config["samples"]), flush=True)
    for variant, checksum in info["executable_sha256"].items():
        if digest(directory / "bin" / (variant + ".exe")) != checksum:
            raise RuntimeError("executable changed during measurement: " + variant)
    return samples


def summarize(samples, variants, workloads, units="calls"):
    result = {}
    for mode, variant in itertools.product(range(len(workloads)), variants):
        group = [s for s in samples if s["mode"] == mode and s["variant"] == variant]
        wall = [s["wall_seconds"] * 1e9 / s[units] for s in group]
        median = statistics.median(wall)
        mad = statistics.median(abs(v - median) for v in wall)
        result[mode, variant] = {
            "workload": workloads[mode], "variant": variant, "rounds": group[0]["rounds"],
            "median_ns": median, "mad_percent": mad / median * 100,
            "min_ns": min(wall), "max_ns": max(wall),
            "median_cpu_ns": statistics.median(s["cpu_seconds"] * 1e9 / s[units] for s in group),
            "median_cpu_wall": statistics.median(s["cpu_seconds"] / s["wall_seconds"] for s in group),
        }
    return result


def write_tables(directory, samples, summary):
    for name, rows in (("samples", samples), ("summary", list(summary.values()))):
        with (directory / (name + ".tsv")).open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)
