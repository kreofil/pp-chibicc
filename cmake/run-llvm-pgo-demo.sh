#!/bin/sh
set -eu
export LC_ALL=C

fail() { echo "LLVM PGO demo: $*" >&2; exit 1; }
[ "$#" -eq 10 ] || fail "expected source, compiler, host CC, llvm-as, opt, llc, output, llvm-link, llvm-profdata, clang"
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
demo=$source_dir/examples/llvm/pgo

llvm_major() {
  "$1" --version | sed -n 's/.*LLVM version \([0-9][0-9]*\)\..*/\1/p' | head -n 1
}
major=$(llvm_major "$llvm_as")
[ -n "$major" ] && [ "$major" -ge 23 ] || fail "this experiment requires LLVM/Clang 23 or newer"
for tool in "$llvm_opt" "$llvm_llc" "$llvm_link" "$llvm_profdata"; do
  [ "$(llvm_major "$tool")" = "$major" ] || fail "all LLVM tools must have the same major version"
done
clang_major=$("$profile_clang" --version | sed -n 's/.*clang version \([0-9][0-9]*\)\..*/\1/p' | head -n 1)
[ "$clang_major" = "$major" ] || fail "profiling Clang and LLVM tools must have the same major version"
opt_version=$("$llvm_opt" --version | sed -n 's/.*LLVM version \([^[:space:]]*\).*/\1/p' | head -n 1)
mkdir -p "$result_dir/source" "$result_dir/input" "$result_dir/objects" "$result_dir/training" "$result_dir/O2" "$result_dir/PGO"
cp "$demo"/* "$result_dir/source/"

emit_module() {
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=asm "$1" -o "$2.s"
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=obj "$1" -o "$2.o"
}

expected_line() {
  printf 'mode=%s n=%s sum=%s visits=%s retagged=%s\n' "$1" "$2" "$3" "$4" "$5"
}

check_cases() (
  executable=$1
  cases=$2
  prefix=$3
  : >"$prefix.stdout"
  : >"$prefix.stderr"
  : >"$prefix.expected"
  while read -r mode count sum visits retagged; do
    "$executable" "$mode" "$count" >>"$prefix.stdout" 2>>"$prefix.stderr" ||
      fail "$executable failed for mode=$mode n=$count"
    expected_line "$mode" "$count" "$sum" "$visits" "$retagged" >>"$prefix.expected"
  done <"$cases"
  cmp "$prefix.expected" "$prefix.stdout" || fail "incorrect results: $executable"
  [ ! -s "$prefix.stderr" ] || fail "unexpected stderr: $executable"
)

expect_status() (
  executable=$1
  mode=$2
  count=$3
  expected=$4
  prefix=$5
  if "$executable" "$mode" "$count" >"$prefix.stdout" 2>"$prefix.stderr"; then status=0; else status=$?; fi
  [ "$status" -eq "$expected" ] || fail "$executable returned $status; expected $expected (mode=$mode n=$count)"
  [ ! -s "$prefix.stdout" ] && [ ! -s "$prefix.stderr" ] || fail "unexpected rejection output"
)

check_program() (
  executable=$1
  prefix=$2
  scenario=$3
  check_cases "$executable" "$demo/common.cases" "$prefix.common"
  if [ "$scenario" = extended ]; then
    check_cases "$executable" "$demo/extension.cases" "$prefix.extension"
  else
    expect_status "$executable" 3 7 3 "$prefix.unavailable-growing"
    expect_status "$executable" 4 7 3 "$prefix.unavailable-switching"
  fi
  "$executable" >"$prefix.default.stdout" 2>"$prefix.default.stderr" || fail "default execution failed"
  expected_line 2 7 67.00 0 0 >"$prefix.default.expected"
  cmp "$prefix.default.expected" "$prefix.default.stdout" || fail "incorrect default output"
  [ ! -s "$prefix.default.stderr" ] || fail "unexpected default stderr"
  expect_status "$executable" -1 7 2 "$prefix.bad-mode-low"
  expect_status "$executable" 6 7 2 "$prefix.bad-mode-high"
  expect_status "$executable" 0 -1 2 "$prefix.bad-count-low"
  expect_status "$executable" 0 4097 2 "$prefix.bad-count-high"
)

# main and extension stay outside the profiled module. The same public
# dispatch_many call site is trained and later receives unseen shapes.
for unit in base handlers main stub extension; do
  "$compiler" -c "$demo/$unit.c" -o "$result_dir/objects/$unit.native.o"
  "$compiler" -S -emit-llvm "$demo/$unit.c" -o "$result_dir/input/$unit.ll"
  "$llvm_as" "$result_dir/input/$unit.ll" -o "$result_dir/input/$unit.bc"
  "$llvm_opt" -passes=verify -disable-output "$result_dir/input/$unit.bc"
done
"$llvm_link" "$result_dir/input/base.bc" "$result_dir/input/handlers.bc" -o "$result_dir/input/core.bc"
for unit in main stub extension; do
  "$llvm_opt" -mcpu=x86-64 -passes='default<O2>' -verify-each \
    "$result_dir/input/$unit.bc" -o "$result_dir/objects/$unit.bc"
  emit_module "$result_dir/objects/$unit.bc" "$result_dir/objects/$unit"
done

echo "LLVM $opt_version: instrumenting core"
"$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-gen-pipeline -passes='default<O2>' -verify-each \
  "$result_dir/input/core.bc" -o "$result_dir/training/core.bc"
"$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-gen-pipeline -passes='default<O2>' \
  -print-pipeline-passes -disable-output "$result_dir/input/core.bc" >"$result_dir/training/passes.txt"
emit_module "$result_dir/training/core.bc" "$result_dir/training/core"
if ! "$profile_clang" -fprofile-generate "$result_dir/training/core.o" \
    "$result_dir/objects/main.o" "$result_dir/objects/stub.o" -o "$result_dir/training/program.exe" \
    >"$result_dir/training/link.stdout" 2>"$result_dir/training/link.stderr"; then
  cat "$result_dir/training/link.stderr" >&2
  fail "cannot link training executable; matching Clang $major and compiler-rt profile runtime are required"
fi
"$profile_clang" -### -fprofile-generate "$result_dir/training/core.o" \
  "$result_dir/objects/main.o" "$result_dir/objects/stub.o" -o "$result_dir/training/program.exe" \
  >"$result_dir/training/link-command.stdout" 2>"$result_dir/training/link-command.txt"
# A fresh directory and an explicit input to merge prevent stale profiles
# from previous invocations from entering this experiment.
profile_run=$(mktemp -d "$result_dir/training/run.XXXXXX")
LLVM_PROFILE_FILE="$profile_run/rectangle.profraw" "$result_dir/training/program.exe" 0 4096 \
  >"$profile_run/stdout" 2>"$profile_run/stderr"
expected_line 0 4096 40960.00 0 0 >"$profile_run/expected"
cmp "$profile_run/expected" "$profile_run/stdout" || fail "incorrect training output"
[ ! -s "$profile_run/stderr" ] || fail "unexpected training stderr"
[ -s "$profile_run/rectangle.profraw" ] || fail "training did not produce a raw profile"
"$llvm_profdata" merge "$profile_run/rectangle.profraw" -o "$result_dir/training/profile.profdata"
"$llvm_profdata" show --all-functions --counts --ic-targets "$result_dir/training/profile.profdata" \
  >"$result_dir/training/profile.txt"
"$llvm_profdata" show --function=dispatch_many --counts --ic-targets "$result_dir/training/profile.profdata" \
  >"$result_dir/training/dispatch-profile.txt"
grep -q '__ppc_compute_Shape_rectangle' "$result_dir/training/dispatch-profile.txt" ||
  fail "the profile contains no rectangle target for dispatch_many"

echo "LLVM $opt_version: O2 without profile"
"$llvm_opt" -mcpu=x86-64 -passes='default<O2>' -verify-each \
  "$result_dir/input/core.bc" -o "$result_dir/O2/core.bc"
"$llvm_opt" -mcpu=x86-64 -passes='default<O2>' -print-pipeline-passes \
  -disable-output "$result_dir/input/core.bc" >"$result_dir/O2/passes.txt"
echo "LLVM $opt_version: profile use and indirect-call promotion"
"$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-use-pipeline \
  -profile-file="$result_dir/training/profile.profdata" -passes='default<O2>' -verify-each \
  -pass-remarks=pgo-icall-prom -pass-remarks-filter=pgo-icall-prom \
  -pass-remarks-output="$result_dir/PGO/promotion.yaml" \
  -print-before=pgo-icall-prom -print-after=pgo-icall-prom -filter-print-funcs=dispatch_many \
  "$result_dir/input/core.bc" -o "$result_dir/PGO/core.bc" \
  >"$result_dir/PGO/use.stdout" 2>"$result_dir/PGO/icp.log"
if grep -Eq '^(warning|error):' "$result_dir/PGO/icp.log"; then
  fail "profile use produced diagnostics; see $result_dir/PGO/icp.log"
fi
"$llvm_opt" -mcpu=x86-64 -pgo-kind=pgo-instr-use-pipeline \
  -profile-file="$result_dir/training/profile.profdata" -passes='default<O2>' \
  -print-pipeline-passes -disable-output "$result_dir/input/core.bc" >"$result_dir/PGO/passes.txt"
promotions=$(awk '/^Name:[ \t]*Promoted/ { n++ } END { print n + 0 }' "$result_dir/PGO/promotion.yaml")

printf 'pipeline\tfunction\tinstructions\talloca\tload\tstore\tphi\tdirect_calls\tindirect_calls\tmemory_intrinsics\n' >"$result_dir/metrics.tsv"
for variant in O2 PGO; do
  prefix=$result_dir/$variant
  # Function-filtered IR dumps are diagnostic excerpts. Serialize the valid
  # bitcode in a separate invocation without the dump filter.
  "$llvm_opt" -S -passes=verify "$prefix/core.bc" -o "$prefix/core.ll"
  "$llvm_as" "$prefix/core.ll" -o "$prefix/roundtrip.bc"
  emit_module "$prefix/core.bc" "$prefix/core"
  "$llvm_opt" -passes='print<func-properties>' -disable-output "$prefix/core.bc" \
    >"$prefix/properties.stdout" 2>"$prefix/properties.txt"
  awk -v pipeline="$variant" -f "$source_dir/cmake/llvm-ir-metrics.awk" \
    "$prefix/core.ll" >>"$result_dir/metrics.tsv"
  awk '
    /^define .*@dispatch_many\(/ { selected = 1 }
    selected { print }
    /^}/ { selected = 0 }
  ' "$prefix/core.ll" >"$prefix/dispatch_many.txt"
  [ -s "$prefix/dispatch_many.txt" ] || fail "missing dispatch_many ($variant)"
done

for scenario in baseline extended; do
  tail_unit=stub
  [ "$scenario" != extended ] || tail_unit=extension
  mkdir -p "$result_dir/$scenario"
  "$host_cc" "$result_dir/objects/base.native.o" "$result_dir/objects/handlers.native.o" \
    "$result_dir/objects/main.native.o" "$result_dir/objects/$tail_unit.native.o" \
    -o "$result_dir/$scenario/native.exe"
  check_program "$result_dir/$scenario/native.exe" "$result_dir/$scenario/native" "$scenario"
  for variant in O2 PGO; do
    # No profile runtime is linked into production executables. The same
    # optimized core and main objects are reused for the unseen extension.
    "$host_cc" "$result_dir/$variant/core.o" "$result_dir/objects/main.o" \
      "$result_dir/objects/$tail_unit.o" -o "$result_dir/$scenario/$variant.exe"
    check_program "$result_dir/$scenario/$variant.exe" "$result_dir/$scenario/$variant" "$scenario"
  done
done

{
  date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
  echo "Compiler: $compiler; host CC: $host_cc; profiling Clang: $profile_clang"
  echo "llvm-as: $llvm_as; opt: $llvm_opt; llc: $llvm_llc; llvm-link: $llvm_link; llvm-profdata: $llvm_profdata"
  echo 'CPU: x86-64; llc: -O=2 -relocation-model=pic; objects: -filetype=obj'
  echo 'Instrumented/optimized module: base.bc + handlers.bc; main and extension are separate uninstrumented objects'
  echo 'Training: mode 0, count 4096, only rectangle; production tests: common.cases + extension.cases'
  echo "Raw profile: $profile_run/rectangle.profraw; merge input: only this file"
  echo 'Generation: opt -pgo-kind=pgo-instr-gen-pipeline -passes=default<O2>'
  echo 'Use: opt -pgo-kind=pgo-instr-use-pipeline -profile-file=training/profile.profdata -passes=default<O2>'
  echo 'Training link: Clang -fprofile-generate; production link: host CC without profile runtime'
  if command -v git >/dev/null 2>&1 && git -C "$source_dir" rev-parse HEAD >/dev/null 2>&1; then
    echo "Repository HEAD: $(git -C "$source_dir" rev-parse HEAD)"
    if [ -n "$(git -C "$source_dir" status --porcelain)" ]; then echo 'Working tree: dirty'; else echo 'Working tree: clean'; fi
  fi
  echo 'Versions:'
  "$host_cc" --version | head -n 1
  "$profile_clang" --version
  for tool in "$llvm_as" "$llvm_opt" "$llvm_llc" "$llvm_link" "$llvm_profdata"; do "$tool" --version; done
  echo 'Checksums (compiler, source, original IR, reused objects and profiles):'
  sha256sum "$compiler" "$demo"/* "$result_dir"/input/*.ll "$result_dir/input/core.bc" \
    "$result_dir/objects/main.o" "$result_dir/objects/stub.o" "$result_dir/objects/extension.o" \
    "$result_dir/O2/core.o" "$result_dir/PGO/core.o" "$result_dir/training/program.exe" \
    "$profile_run/rectangle.profraw" "$result_dir/training/profile.profdata"
} >"$result_dir/manifest.txt"
{
  echo '# Профиль выполнения и диспетчеризация 4П'
  echo
  echo "LLVM (opt): $opt_version. Все проверки выполнения пройдены."
  echo "Проход pgo-icall-prom сообщил продвижений: $promotions."
  echo 'Профиль собран только на прямоугольниках. Проверены другой обработчик, смешанные входы,'
  echo 'новая специализация с изменением данных и смена специализации объекта между итерациями.'
  echo 'Это проверка преобразования и корректности; время выполнения не измеряется.'
  echo
  awk -F '\t' '
    NR == 1 {
      print "| Вариант | Функция | Инструкции | load | store | Прямые вызовы | Косвенные вызовы |"
      print "| --- | --- | ---: | ---: | ---: | ---: | ---: |"
      next
    }
    $2 == "dispatch_many" { printf "| %s | %s | %s | %s | %s | %s | %s |\n", $1, $2, $3, $5, $6, $8, $9 }
  ' "$result_dir/metrics.tsv"
  echo
  echo 'При продвижении остаётся общий косвенный путь; прямой вызов частого обработчика может быть встроен.'
  echo 'Данные профиля: training/dispatch-profile.txt. Сообщения LLVM: PGO/promotion.yaml.'
  echo 'IR до/после продвижения: PGO/icp.log; полный конечный IR: PGO/core.ll.'
  echo 'Параметры, версии и контрольные суммы: manifest.txt; раскрытые pipeline: training/passes.txt, O2/passes.txt, PGO/passes.txt.'
} >"$result_dir/report.md"
echo "LLVM PGO demo passed; report: $result_dir/report.md"
