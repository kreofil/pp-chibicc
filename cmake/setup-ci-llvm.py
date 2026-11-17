#!/usr/bin/env python3
"""Extract checksum-pinned official LLVM packages without root or APT changes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=Path(__file__).with_name("ci-llvm23.json"))
    parser.add_argument("--prefix", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "linux" or platform.machine() not in ("x86_64", "amd64"):
        parser.error("this LLVM bundle requires Linux x86-64")
    for tool in ("curl", "dpkg-deb"):
        if not shutil.which(tool):
            parser.error(tool + " is required")
    lock = json.loads(args.lock.read_text())
    if not re.fullmatch(r"\d+\.\d+\.\d+", lock["version"]):
        parser.error("invalid LLVM version in lock file")
    major = lock["version"].split(".")[0]
    prefix = args.prefix.resolve()
    packages = prefix / "packages"
    packages.mkdir(parents=True, exist_ok=True)
    for entry in lock["packages"]:
        name, url, checksum = entry["name"], entry["url"], entry["sha256"]
        if not re.fullmatch(r"[a-z0-9+-]+", name) or not re.fullmatch(r"[0-9a-f]{64}", checksum):
            parser.error("invalid package identity/checksum")
        if not url.startswith("https://apt.llvm.org/noble/pool/"):
            parser.error("expected an official LLVM Ubuntu Noble package URL")
        archive = packages / (name + ".deb")
        if not archive.exists() or digest(archive) != checksum:
            partial = archive.with_suffix(".part")
            subprocess.run(["curl", "-fLsS", "--retry", "2", "--connect-timeout", "15",
                            "--max-time", "120", url, "-o", str(partial)], check=True)
            if digest(partial) != checksum:
                partial.unlink()
                raise RuntimeError("SHA256 mismatch for " + name)
            partial.replace(archive)
        subprocess.run(["dpkg-deb", "-x", str(archive), str(prefix)], check=True)
        print("Verified and extracted " + name, flush=True)
    wrappers = prefix / "bin"
    wrappers.mkdir(exist_ok=True)
    for tool in ("llvm-as", "opt", "llc", "llvm-link"):
        executable = prefix / "usr/lib" / ("llvm-" + major) / "bin" / tool
        script = ("#!/bin/sh\nci_llvm_root=" + shlex.quote(str(prefix)) + "\n"
                  'LD_LIBRARY_PATH="$ci_llvm_root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" '
                  "exec " + shlex.quote(str(executable)) + ' "$@"\n')
        for spelling in (tool, tool + "-" + major):
            wrapper = wrappers / spelling
            wrapper.write_text(script)
            wrapper.chmod(0o755)
        version = subprocess.check_output([str(wrappers / tool), "--version"], text=True)
        match = re.search(r"LLVM version (\d+\.\d+\.\d+)", version)
        if not match or match[1] != lock["version"]:
            raise RuntimeError("unexpected version of " + tool + ": " + version)
        print(tool + ": " + match[1], flush=True)
    (prefix / "manifest.json").write_text(json.dumps(lock, indent=2) + "\n")
    print("LLVM tools: " + str(wrappers), flush=True)


if __name__ == "__main__":
    main()
