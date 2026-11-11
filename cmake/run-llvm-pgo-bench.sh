#!/bin/sh
set -eu
export LC_ALL=C
export PYTHONDONTWRITEBYTECODE=1

fail() { echo "LLVM PGO benchmark: $*" >&2; exit 1; }
[ "$#" -eq 11 ] || fail "expected PGO demo arguments followed by Python 3"
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
host_cc=$3
llvm_opt=$5
llvm_llc=$6
mkdir -p "$7"
result_dir=$(realpath "$7")
python=${11}
bench=$source_dir/examples/llvm/pgo-bench
rm -f "$result_dir/report.md" "$result_dir/samples.json" "$result_dir/samples.tsv" "$result_dir/summary.tsv"
"$python" "$source_dir/cmake/llvm-pgo-bench.py" --check-config

# Rebuild and validate the existing study. Its uninstrumented O2/PGO core
# objects are used directly; the benchmark cannot accidentally time training.
"$source_dir/cmake/run-llvm-pgo-demo.sh" "$1" "$2" "$3" "$4" "$5" "$6" \
  "$result_dir/demo" "$8" "$9" "${10}"
mkdir -p "$result_dir/source" "$result_dir/objects" "$result_dir/bin"
cp "$bench"/* "$result_dir/source/"
cp "$source_dir/cmake/llvm-pgo-bench.py" "$result_dir/source/"
cp "$source_dir/cmake/llvm_bench.py" "$result_dir/source/"
"$compiler" -c "$bench/setup.c" -o "$result_dir/objects/setup.o"
"$host_cc" -std=c11 -O2 -Wall -Wextra -Werror -fno-lto \
  -c "$bench/timer.c" -o "$result_dir/objects/timer.o"
"$host_cc" "$result_dir/objects/timer.o" "$result_dir/objects/setup.o" \
  "$result_dir/demo/objects/base.native.o" "$result_dir/demo/objects/handlers.native.o" \
  "$result_dir/demo/objects/extension.native.o" -o "$result_dir/bin/native.exe"
for variant in O2 PGO; do
  "$host_cc" "$result_dir/objects/timer.o" "$result_dir/objects/setup.o" \
    "$result_dir/demo/$variant/core.o" "$result_dir/demo/objects/extension.o" \
    -o "$result_dir/bin/$variant.exe"
done
{
  date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
  echo "Host timer object: $host_cc -std=c11 -O2 -fno-lto; shared by all three binaries"
  echo 'Setup: ppchibicc native, shared by all three binaries, outside timed region'
  echo 'Kernel/handlers: existing demo objects; no instrumentation or LTO in measured binaries'
  echo 'LLVM CPU: x86-64; llc: -O=2 -relocation-model=pic -filetype=obj'
  echo 'Full compiler/tool/source/profile provenance: demo/manifest.txt'
  "$python" --version
  uname -a
  if command -v lscpu >/dev/null 2>&1; then lscpu; fi
  echo 'Checksums:'
  sha256sum "$bench"/* "$source_dir/cmake/llvm-pgo-bench.py" "$source_dir/cmake/llvm_bench.py" \
    "$source_dir/cmake/run-llvm-pgo-bench.sh" "$result_dir"/objects/*.o \
    "$result_dir"/bin/*.exe
} >"$result_dir/manifest.txt"
"$python" "$source_dir/cmake/llvm-pgo-bench.py" "$result_dir"
echo "LLVM PGO benchmark passed; report: $result_dir/report.md"
