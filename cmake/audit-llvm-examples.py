#!/usr/bin/env python3
"""Audit standalone CMake examples without changing their source/configuration."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import time


def command(args, log, env, timeout=90):
    with log.open("w", encoding="utf-8") as stream:
        stream.write("Command: " + shlex.join([str(a) for a in args]) + "\n")
        stream.flush()
        try:
            result = subprocess.run([str(a) for a in args], stdout=stream, stderr=subprocess.STDOUT,
                                    env=env, timeout=timeout)
            return result.returncode
        except subprocess.TimeoutExpired:
            stream.write("\nAudit timeout\n")
            return 124


def diagnostic(log):
    text = log.read_text(errors="replace")
    match = re.search(r"LLVM backend: ([^\n]+)", text)
    if match:
        return "LLVM: " + match[1]
    for line in text.splitlines():
        if re.search(r"CMake Error|error:|undefined reference|unknown argument:|unknown file extension:|expected |Audit timeout", line):
            return line.strip()[:180]
    return "see " + str(log)


def executable(build):
    reply = build / ".cmake/api/v1/reply"
    indexes = sorted(reply.glob("index-*.json"))
    if not indexes:
        return None
    index = json.loads(indexes[-1].read_text())
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text())
    programs = []
    for target in model["configurations"][0]["targets"]:
        info = json.loads((reply / target["jsonFile"]).read_text())
        if info["type"] == "EXECUTABLE":
            programs += [(build / a["path"]).resolve() for a in info.get("artifacts", [])]
    return programs[0] if len(programs) == 1 else None


def fixture_directory(source):
    if "direct-classic" in source.parts:
        return source / "data"
    if source.parent.name == "cmake" and source.parent.parent.name == "evo-reuse-code":
        return source.parent.parent / source.name / "data"
    return None


def write_report(output, info):
    counts = Counter(r["llvm"]["status"] for r in info["results"])
    report = ["# Аудит CMake-примеров через LLVM", "", "UTC: " + info["utc"],
              "Результаты: " + str(dict(counts)), "",
              "Проверяется O2. После ошибки LLVM выполняется native-сборка для сравнения. "
              "Успешная сборка без входного эталона не означает проверку выполнения.",
              "Эталоны фигур сравниваются с нормализацией CRLF/LF. Свежие артефакты: " + info["run"], "",
              "| Проект | LLVM | Native при ошибке LLVM | Причина |", "| --- | --- | --- | --- |"]
    for record in info["results"]:
        llvm = record["llvm"]
        report.append("| {} | {} | {} | {} |".format(record["project"], llvm["status"],
                      record.get("x86-64", {}).get("status", "—"), llvm.get("diagnostic", "—").replace("|", "\\|")))
    (output / "report.md").write_text("\n".join(report) + "\n", encoding="utf-8")


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=str(root / "bin/ppchibicc"))
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--output", default=str(root / "build-llvm-example-audit"))
    args = parser.parse_args()
    compiler = Path(args.compiler).resolve()
    cmake = shutil.which(args.cmake)
    if not compiler.is_file() or not cmake:
        parser.error("compiler and cmake must be available")
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    versions = {}
    for key, default in (("PPCHIBICC_LLVM_OPT", "opt"), ("PPCHIBICC_LLVM_LLC", "llc")):
        tool = os.environ.get(key) or default
        version = subprocess.check_output([tool, "--version"], text=True)
        versions[key] = {"path": tool, "version": version}
    majors = [re.search(r"LLVM version (\d+)\.", v["version"]) for v in versions.values()]
    if any(m is None for m in majors) or len({m[1] for m in majors}) != 1 or int(majors[0][1]) < 19:
        parser.error("opt and llc must have matching major versions, LLVM 19+")
    tracked = subprocess.run(["git", "ls-files", "-z", "--", "examples"], cwd=root,
                             capture_output=True) if shutil.which("git") else None
    if tracked is not None and tracked.returncode == 0:
        cmake_files = [root / os.fsdecode(p) for p in tracked.stdout.split(b"\0")
                       if p.endswith(b"/CMakeLists.txt")]
    else:
        cmake_files = [p for p in (root / "examples").rglob("CMakeLists.txt")
                       if not any(part.startswith("build") or part == "CMakeFiles" for part in p.parts)]
    projects = [p.parent for p in sorted(cmake_files)
                if re.search(r"^\s*project\s*\(", p.read_text(), re.M | re.I)]
    info = {"utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "run": str(run),
            "compiler": str(compiler), "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
            "tools": versions, "results": []}
    if shutil.which("git"):
        info["repository_head"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
        info["working_tree_dirty"] = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True))
    for number, source in enumerate(projects, 1):
        record = {"project": str(source.relative_to(root))}
        for mode in ("llvm", "x86-64"):
            # Native is a control only for examples that failed through LLVM.
            if mode == "x86-64" and record["llvm"]["status"] == "passed":
                break
            directory = run / ("{:02d}".format(number)) / mode
            wrapper_dir = directory / "bin"
            wrapper_dir.mkdir(parents=True)
            wrapper = wrapper_dir / "ppchibicc"
            wrapper.write_text("#!/bin/sh\nexec " + shlex.quote(str(compiler)) +
                               ' "$@" --backend=' + mode + " -O2\n")
            wrapper.chmod(0o755)
            env = dict(os.environ, LC_ALL="C", PATH=str(wrapper_dir) + os.pathsep + os.environ["PATH"])
            # Legacy examples place outputs several levels above CMAKE_BINARY_DIR.
            # Keep those paths within a separate tree for each project and mode.
            build = directory / "work/a/b/c/build"
            query = build / ".cmake/api/v1/query"
            query.mkdir(parents=True)
            (query / "codemodel-v2").touch()
            log = directory / "configure.log"
            status = command([cmake, "-S", source, "-B", build,
                              "-DCMAKE_C_COMPILER=" + str(wrapper)], log, env)
            result = {"build": str(build), "status": "configure-failed", "log": str(log)}
            if not status:
                log = directory / "build.log"
                status = command([cmake, "--build", build, "--parallel", "2"], log, env)
                result.update(status="build-failed", log=str(log))
            if not status:
                result["status"] = "passed"
                data = fixture_directory(source)
                program = executable(build)
                if data and (data / "input1.txt").exists() and (data / "output1.txt").exists() and program:
                    actual = directory / "output1.txt"
                    status = command([program, data / "input1.txt", actual], directory / "run.log", env, 15)
                    expected = (data / "output1.txt").read_bytes().replace(b"\r\n", b"\n")
                    matched = not status and actual.exists() and actual.read_bytes().replace(b"\r\n", b"\n") == expected
                    result["fixture"] = "passed" if matched else "failed"
                    if not matched:
                        result["status"] = "fixture-failed"
                        result["diagnostic"] = "output differs or program failed; see " + str(directory / "run.log")
            if status and result["status"] != "fixture-failed":
                result["diagnostic"] = diagnostic(log)
            record[mode] = result
        info["results"].append(record)
        (output / "results.json").write_text(json.dumps(info, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print("[{}/{}] {}: {}{}".format(number, len(projects), record["project"], record["llvm"]["status"],
              "; native " + record["x86-64"]["status"] if "x86-64" in record else ""), flush=True)
    write_report(output, info)
    print("Audit report: " + str(output / "report.md"), flush=True)


if __name__ == "__main__":
    main()
