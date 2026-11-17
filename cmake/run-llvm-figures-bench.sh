#!/bin/sh
set -eu
export LC_ALL=C
export PYTHONDONTWRITEBYTECODE=1

fail() { echo "LLVM figures benchmark: $*" >&2; exit 1; }
[ "$#" -eq 11 ] || fail "expected source, compiler, host CC, llvm-as, opt, llc, output, llvm-link, llvm-profdata, clang, Python"
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
host_cc=$3
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
mkdir -p "$7"
result_dir=$(realpath "$7")
llvm_link=$8
llvm_profdata=$9
profile_clang=${10}
python=${11}
runner=$source_dir/cmake/llvm-figures-bench.py
library=$source_dir/examples/containers/vector-list-lib
figure_dir=$library/examples/ppvector-figures
rm -f "$result_dir/report.md" "$result_dir/samples.json" "$result_dir/samples.tsv" "$result_dir/summary.tsv"
"$python" "$runner" --check-config

llvm_major() {
  "$1" --version | sed -n 's/.*LLVM version \([0-9][0-9]*\)\..*/\1/p' | head -n 1
}
major=$(llvm_major "$llvm_as")
[ -n "$major" ] && [ "$major" -ge 23 ] || fail "LLVM/Clang 23 or newer is required"
for tool in "$llvm_opt" "$llvm_llc" "$llvm_link" "$llvm_profdata"; do
  [ "$(llvm_major "$tool")" = "$major" ] || fail "all LLVM tools must have the same major version"
done
clang_major=$("$profile_clang" --version | sed -n 's/.*clang version \([0-9][0-9]*\)\..*/\1/p' | head -n 1)
[ "$clang_major" = "$major" ] || fail "profiling Clang and LLVM tools must have the same major version"

mkdir -p "$result_dir/source/library/include" "$result_dir/source/figures" "$result_dir/source/bench" \
  "$result_dir/objects" "$result_dir/bin" "$result_dir/original" "$result_dir/training"
cp "$library/src/ppvector/ppvector.c" "$library/src/ppvector/ppgeneral.c" "$result_dir/source/library/"
cp "$library/src/include/ppvector.h" "$result_dir/source/library/include/"
cp -R "$library/src/include/ppvector" "$result_dir/source/library/include/"
cp "$figure_dir"/*.c "$figure_dir"/*.h "$result_dir/source/figures/"
cp "$source_dir/examples/llvm/figures-bench/"* "$result_dir/source/bench/"
cp "$runner" "$source_dir/cmake/llvm_bench.py" "$result_dir/source/"
"$python" "$runner" --prepare "$result_dir"

compile_unit() {
  unit_source=$1
  group=$2
  name=$3
  mkdir -p "$result_dir/input/$group" "$result_dir/objects/native/$group"
  "$compiler" -I"$result_dir/source/library/include" -I"$result_dir/source/figures" \
    -c "$unit_source" -o "$result_dir/objects/native/$group/$name.o"
  "$compiler" -I"$result_dir/source/library/include" -I"$result_dir/source/figures" \
    -S -emit-llvm "$unit_source" -o "$result_dir/input/$group/$name.ll"
  "$llvm_as" "$result_dir/input/$group/$name.ll" -o "$result_dir/input/$group/$name.bc"
  "$llvm_opt" -passes=verify -disable-output "$result_dir/input/$group/$name.bc"
}

emit_module() {
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=asm "$1" -o "$2.s"
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=obj "$1" -o "$2.o"
}

for unit in ppvector ppgeneral; do
  compile_unit "$result_dir/source/library/$unit.c" library "$unit"
done
for unit in figure-perimeter figure-rectangle-perimeter figure-triangle-perimeter rectangle-perimeter triangle-perimeter; do
  compile_unit "$result_dir/source/figures/$unit.c" library "$unit"
done
compile_unit "$result_dir/source/bench/collection.c" bench collection
"$llvm_link" "$result_dir"/input/library/*.bc -o "$result_dir/input/library.bc"
"$llvm_link" "$result_dir/input/library.bc" "$result_dir/input/bench/collection.bc" -o "$result_dir/input/core.bc"

# First run the existing ten-figure program, including its original printing
# and sorting handlers, without the benchmark adapter.
for unit in figure-out figure-rectangle-out figure-triangle-out rectangle-out triangle-out ppvector-figures; do
  compile_unit "$result_dir/source/figures/$unit.c" original "$unit"
done
"$host_cc" "$result_dir"/objects/native/library/*.o "$result_dir"/objects/native/original/*.o \
  -o "$result_dir/original/native.exe"
"$llvm_link" "$result_dir/input/library.bc" "$result_dir"/input/original/*.bc -o "$result_dir/original/input.bc"
"$llvm_opt" -mcpu=x86-64 -passes='default<O2>' -verify-each "$result_dir/original/input.bc" \
  -o "$result_dir/original/O2.bc"
emit_module "$result_dir/original/O2.bc" "$result_dir/original/O2"
"$host_cc" "$result_dir/original/O2.o" -o "$result_dir/original/O2.exe"
for variant in native O2; do
  "$result_dir/original/$variant.exe" >"$result_dir/original/$variant.stdout" 2>"$result_dir/original/$variant.stderr"
  [ ! -s "$result_dir/original/$variant.stderr" ] || fail "original program produced stderr"
done
cmp "$result_dir/original/native.stdout" "$result_dir/original/O2.stdout" || fail "original program outputs differ"
echo 'Existing ppvector-figures: native and LLVM O2 outputs agree'

"$host_cc" -std=c11 -O2 -Wall -Wextra -Werror -fno-lto -c "$result_dir/source/bench/timer.c" \
  -o "$result_dir/objects/timer.o"
"$host_cc" "$result_dir/objects/timer.o" "$result_dir"/objects/native/library/*.o \
  "$result_dir/objects/native/bench/collection.o" -o "$result_dir/bin/native.exe"

echo "LLVM $major: instrumenting collection workflow"
"$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-gen-pipeline -passes='default<O2>' -verify-each \
  "$result_dir/input/core.bc" -o "$result_dir/training/core.bc"
"$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-gen-pipeline -passes='default<O2>' \
  -print-pipeline-passes -disable-output "$result_dir/input/core.bc" >"$result_dir/training/passes.txt"
emit_module "$result_dir/training/core.bc" "$result_dir/training/core"
if ! "$profile_clang" -fprofile-generate "$result_dir/training/core.o" "$result_dir/objects/timer.o" \
    -o "$result_dir/training/program.exe" >"$result_dir/training/link.stdout" 2>"$result_dir/training/link.stderr"; then
  cat "$result_dir/training/link.stderr" >&2
  fail "cannot link training executable; Clang $major and matching compiler-rt profile runtime are required"
fi
"$profile_clang" -### -fprofile-generate "$result_dir/training/core.o" "$result_dir/objects/timer.o" \
  -o "$result_dir/training/program.exe" >"$result_dir/training/link-command.stdout" 2>"$result_dir/training/link-command.txt"
profile_run=$(mktemp -d "$result_dir/training/run.XXXXXX")
for variant in PGO90 PGO50; do
  dataset=train90
  [ "$variant" != PGO50 ] || dataset=train50
  mkdir -p "$result_dir/training/$variant"
  LLVM_PROFILE_FILE="$profile_run/$dataset.profraw" "$result_dir/training/program.exe" \
    "$result_dir/data/$dataset.txt" 2 >"$result_dir/training/$dataset.stdout" 2>"$result_dir/training/$dataset.stderr"
  [ ! -s "$result_dir/training/$dataset.stderr" ] || fail "training produced stderr"
  "$llvm_profdata" merge "$profile_run/$dataset.profraw" -o "$result_dir/training/$variant/profile.profdata"
  "$llvm_profdata" show --all-functions --counts --ic-targets "$result_dir/training/$variant/profile.profdata" \
    >"$result_dir/training/$variant/profile.txt"
done

printf 'pipeline\tfunction\tinstructions\talloca\tload\tstore\tphi\tdirect_calls\tindirect_calls\tmemory_intrinsics\n' >"$result_dir/metrics.tsv"
for variant in O2 PGO90 PGO50; do
  mkdir -p "$result_dir/$variant"
  echo "LLVM $major: $variant collection"
  if [ "$variant" = O2 ]; then
    "$llvm_opt" -mcpu=x86-64 -passes='default<O2>' -verify-each "$result_dir/input/core.bc" \
      -o "$result_dir/$variant/core.bc"
    "$llvm_opt" -mcpu=x86-64 -passes='default<O2>' -print-pipeline-passes -disable-output \
      "$result_dir/input/core.bc" >"$result_dir/$variant/passes.txt"
  else
    "$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-use-pipeline \
      -profile-file="$result_dir/training/$variant/profile.profdata" -passes='default<O2>' -verify-each \
      -pass-remarks=pgo-icall-prom -pass-remarks-filter=pgo-icall-prom \
      -pass-remarks-output="$result_dir/$variant/promotion.yaml" \
      "$result_dir/input/core.bc" -o "$result_dir/$variant/core.bc" \
      >"$result_dir/$variant/use.stdout" 2>"$result_dir/$variant/use.stderr"
    if grep -Eq '^(warning|error):' "$result_dir/$variant/use.stderr"; then
      fail "profile use produced diagnostics; see $result_dir/$variant/use.stderr"
    fi
    "$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-use-pipeline \
      -profile-file="$result_dir/training/$variant/profile.profdata" -passes='default<O2>' \
      -print-pipeline-passes -disable-output "$result_dir/input/core.bc" >"$result_dir/$variant/passes.txt"
  fi
  "$llvm_opt" -S -passes=verify "$result_dir/$variant/core.bc" -o "$result_dir/$variant/core.ll"
  "$llvm_as" "$result_dir/$variant/core.ll" -o "$result_dir/$variant/roundtrip.bc"
  emit_module "$result_dir/$variant/core.bc" "$result_dir/$variant/core"
  "$host_cc" "$result_dir/objects/timer.o" "$result_dir/$variant/core.o" -o "$result_dir/bin/$variant.exe"
  awk -v pipeline="$variant" -f "$source_dir/cmake/llvm-ir-metrics.awk" "$result_dir/$variant/core.ll" \
    >>"$result_dir/metrics.tsv"
done

{
  date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
  echo "Compiler: $compiler; host CC: $host_cc; profiling Clang: $profile_clang"
  echo 'CPU: x86-64; llc: -O=2 -relocation-model=pic -filetype=obj'
  echo 'Shared timer: host CC -std=c11 -O2 -fno-lto. No instrumentation in measured binaries.'
  echo 'Core: ppVector library, original perimeter functions and collection adapter; no internalize.'
  echo 'Training: train90/seed 1 and train50/seed 2, 4096 objects, 2 warmup + 2 measured rounds.'
  echo "Fresh raw profile directory: $profile_run; each merged profile has exactly one input"
  echo 'Evaluation: new seeds and sizes; see datasets.json for hashes and independent checksums.'
  if command -v git >/dev/null 2>&1 && git -C "$source_dir" rev-parse HEAD >/dev/null 2>&1; then
    echo "Repository HEAD: $(git -C "$source_dir" rev-parse HEAD)"
    if [ -n "$(git -C "$source_dir" status --porcelain)" ]; then echo 'Working tree: dirty'; else echo 'Working tree: clean'; fi
  fi
  "$host_cc" --version | head -n 1
  "$profile_clang" --version
  for tool in "$llvm_as" "$llvm_opt" "$llvm_llc" "$llvm_link" "$llvm_profdata"; do "$tool" --version; done
  "$python" --version
  uname -a
  if command -v lscpu >/dev/null 2>&1; then lscpu; fi
  if command -v ldd >/dev/null 2>&1; then ldd --version | sed -n '1p'; fi
  echo 'Checksums:'
  sha256sum "$compiler" "$runner" "$source_dir/cmake/llvm_bench.py" "$source_dir/cmake/run-llvm-figures-bench.sh" \
    "$result_dir"/source/bench/* "$result_dir"/source/library/*.c \
    "$result_dir/source/library/include/ppvector.h" "$result_dir"/source/library/include/ppvector/* \
    "$result_dir"/source/figures/* "$result_dir"/data/*.txt "$result_dir/input/core.bc" \
    "$result_dir/objects/timer.o" "$result_dir"/bin/*.exe "$result_dir/training/program.exe" \
    "$profile_run"/*.profraw "$result_dir"/training/PGO*/profile.profdata
} >"$result_dir/manifest.txt"
"$python" "$runner" "$result_dir"
echo "LLVM figures benchmark passed; report: $result_dir/report.md"
