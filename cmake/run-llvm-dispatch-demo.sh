#!/bin/sh
set -eu
export LC_ALL=C

source_dir=$(realpath "$1")
compiler=$(realpath "$2")
host_cc=$3
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
mkdir -p "$7"
result_dir=$(realpath "$7")
llvm_link=$8

fail() { echo "LLVM dispatch demo: $*" >&2; exit 1; }
llvm_major() {
  "$1" --version | sed -n 's/.*LLVM version \([0-9][0-9]*\)\..*/\1/p' | head -n 1
}
major=$(llvm_major "$llvm_as")
[ -n "$major" ] && [ "$major" -ge 19 ] || fail "LLVM 19 or newer is required"
for tool in "$llvm_opt" "$llvm_llc" "$llvm_link"; do
  [ "$(llvm_major "$tool")" = "$major" ] ||
    fail "llvm-as, opt, llc and llvm-link must have the same major version"
done
opt_version=$("$llvm_opt" --version | sed -n 's/.*LLVM version \([^[:space:]]*\).*/\1/p' | head -n 1)
demo=$source_dir/examples/llvm/dispatch
roots=main,probe_known,probe_closed,probe_open,probe_control,extra_probe
mkdir -p "$result_dir/source" "$result_dir/input" "$result_dir/native"
cp "$demo"/* "$result_dir/source/"

check_execution() {
  execution_exe=$1
  execution_prefix=$2
  execution_expected=$3
  : >"$execution_prefix.stdout"
  : >"$execution_prefix.stderr"
  for count in 0 1 4 7 31; do
    "$execution_exe" "$count" >>"$execution_prefix.stdout" 2>>"$execution_prefix.stderr" ||
      fail "$execution_exe failed for n=$count"
  done
  cmp "$execution_expected" "$execution_prefix.stdout" || fail "incorrect output: $execution_exe"
  [ ! -s "$execution_prefix.stderr" ] || fail "unexpected stderr: $execution_exe"
  "$execution_exe" >"$execution_prefix.default.stdout" 2>"$execution_prefix.default.stderr" ||
    fail "$execution_exe failed without arguments"
  awk '$1 == "n=4"' "$execution_expected" >"$execution_prefix.default.expected"
  cmp "$execution_prefix.default.expected" "$execution_prefix.default.stdout" || fail "incorrect default output"
  [ ! -s "$execution_prefix.default.stderr" ] || fail "unexpected default stderr"
  for count in -1 33; do
    if "$execution_exe" "$count" >"$execution_prefix.rejected.stdout" 2>"$execution_prefix.rejected.stderr"; then
      fail "$execution_exe accepted n=$count"
    else
      status=$?
    fi
    [ "$status" -eq 2 ] || fail "$execution_exe returned $status for n=$count"
    [ ! -s "$execution_prefix.rejected.stdout" ] && [ ! -s "$execution_prefix.rejected.stderr" ] ||
      fail "unexpected output for n=$count"
  done
}

emit_module() {
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=asm "$1" -o "$2.s"
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=obj "$1" -o "$2.o"
}

# Compile each unit once. The extension later replaces only empty.o; the five
# core objects, including callers and the base generic handler, are reused.
for unit in local base rectangle square main empty extension; do
  "$compiler" -c "$demo/$unit.c" -o "$result_dir/native/$unit.o"
  "$compiler" -S -emit-llvm "$demo/$unit.c" -o "$result_dir/input/$unit.ll"
  "$llvm_as" "$result_dir/input/$unit.ll" -o "$result_dir/input/$unit.bc"
  "$llvm_opt" -passes=verify -disable-output "$result_dir/input/$unit.bc"
  for mode in raw unit-O2; do
    prefix=$result_dir/units/$mode/$unit
    mkdir -p "$result_dir/units/$mode"
    passes=verify
    [ "$mode" != unit-O2 ] || passes='default<O2>'
    "$llvm_opt" -mcpu=x86-64 -S -passes="$passes" -verify-each \
      "$result_dir/input/$unit.bc" -o "$prefix.ll"
    "$llvm_as" "$prefix.ll" -o "$prefix.bc"
    emit_module "$prefix.bc" "$prefix"
  done
done

printf 'pipeline\tfunction\tinstructions\talloca\tload\tstore\tphi\tdirect_calls\tindirect_calls\tmemory_intrinsics\n' >"$result_dir/metrics.tsv"
printf 'set\tmode\tunit_passes\tlinked_passes\tpublic_api\tllc_options\n' >"$result_dir/pipelines.tsv"

for scenario in baseline extended; do
  tail_unit=empty
  expected=$demo/expected.txt
  if [ "$scenario" = extended ]; then
    tail_unit=extension
    expected=$demo/extended.expected.txt
  fi
  set --
  for unit in local base rectangle square main "$tail_unit"; do
    set -- "$@" "$result_dir/native/$unit.o"
  done
  "$host_cc" "$@" -o "$result_dir/native/$scenario.exe"
  check_execution "$result_dir/native/$scenario.exe" "$result_dir/native/$scenario" "$expected"

  for mode in raw unit-O2 linked-O2 internalized-O2; do
    prefix=$result_dir/$scenario/$mode
    mkdir -p "$prefix"
    echo "LLVM $opt_version: $scenario / $mode"
    input_mode=raw
    unit_passes=verify
    [ "$mode" != unit-O2 ] || input_mode=unit-O2
    [ "$mode" != unit-O2 ] || unit_passes='default<O2>'
    set --
    for unit in local base rectangle square main "$tail_unit"; do
      set -- "$@" "$result_dir/units/$input_mode/$unit.bc"
    done
    # For raw/unit-O2 this is an audit copy only. Executables below link the
    # separate objects; no optimizer sees the combined audit copy.
    "$llvm_link" "$@" -o "$prefix/linked.bc"
    passes=verify
    public_api=-
    case $mode in
      linked-O2) passes='default<O2>' ;;
      internalized-O2) passes='internalize,default<O2>'; public_api=$roots ;;
    esac
    printf '%s\t%s\t%s\t%s\t%s\t-O=2 -mcpu=x86-64 -relocation-model=pic\n' \
      "$scenario" "$mode" "$unit_passes" "$passes" "$public_api" >>"$result_dir/pipelines.tsv"
    if [ "$mode" = internalized-O2 ]; then
      "$llvm_opt" -mcpu=x86-64 -S -passes="$passes" -internalize-public-api-list="$roots" \
        -verify-each "$prefix/linked.bc" -o "$prefix/program.ll"
      "$llvm_opt" -mcpu=x86-64 -passes="$passes" -internalize-public-api-list="$roots" \
        -print-pipeline-passes -disable-output "$prefix/linked.bc" >"$prefix/linked-passes.txt"
    else
      "$llvm_opt" -mcpu=x86-64 -S -passes="$passes" -verify-each \
        "$prefix/linked.bc" -o "$prefix/program.ll"
      "$llvm_opt" -mcpu=x86-64 -passes="$passes" -print-pipeline-passes \
        -disable-output "$prefix/linked.bc" >"$prefix/linked-passes.txt"
    fi
    "$llvm_opt" -mcpu=x86-64 -passes="$unit_passes" -print-pipeline-passes \
      -disable-output "$result_dir/input/local.bc" >"$prefix/unit-passes.txt"
    "$llvm_as" "$prefix/program.ll" -o "$prefix/program.bc"
    "$llvm_opt" -passes='print<func-properties>' -disable-output \
      "$prefix/program.bc" >"$prefix/properties.stdout" 2>"$prefix/properties.txt"
    if [ "$mode" = raw ] || [ "$mode" = unit-O2 ]; then
      set --
      for unit in local base rectangle square main "$tail_unit"; do
        set -- "$@" "$result_dir/units/$mode/$unit.o"
      done
    else
      emit_module "$prefix/program.bc" "$prefix/program"
      set -- "$prefix/program.o"
    fi
    "$host_cc" "$@" -o "$prefix/program.exe"
    check_execution "$prefix/program.exe" "$prefix/result" "$expected"
    awk -v pipeline="$scenario/$mode" -f "$source_dir/cmake/llvm-ir-metrics.awk" \
      "$prefix/program.ll" >>"$result_dir/metrics.tsv"
    for function in probe_known probe_closed probe_open probe_control extra_probe main; do
      awk -v wanted="$function" '
        /^define / {
          name = $0; sub(/^[^@]*@/, "", name); sub(/\(.*/, "", name); gsub(/"/, "", name)
          selected = name == wanted
        }
        selected { print }
        /^}/ { selected = 0 }
      ' "$prefix/program.ll" >"$prefix/$function.txt"
      [ -s "$prefix/$function.txt" ] || fail "missing public function $function ($scenario/$mode)"
    done
  done
done

{
  date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
  echo "Compiler: $compiler; host CC: $host_cc"
  echo "llvm-as: $llvm_as; opt: $llvm_opt; llc: $llvm_llc; llvm-link: $llvm_link"
  echo 'CPU: x86-64; llc: -O=2 -relocation-model=pic; objects: -filetype=obj'
  echo "Internalized public API: $roots"
  echo 'Inputs: 0 1 4 7 31; default: 4; rejected inputs: -1 33 (exit 2)'
  if command -v git >/dev/null 2>&1 && git -C "$source_dir" rev-parse HEAD >/dev/null 2>&1; then
    echo "Repository HEAD: $(git -C "$source_dir" rev-parse HEAD)"
    if [ -n "$(git -C "$source_dir" status --porcelain)" ]; then echo 'Working tree: dirty'; else echo 'Working tree: clean'; fi
  fi
  echo 'Versions:'
  "$host_cc" --version | head -n 1
  for tool in "$llvm_as" "$llvm_opt" "$llvm_llc" "$llvm_link"; do "$tool" --version; done
  echo 'Checksums (compiler, source files, original IR and reused native/LLVM objects):'
  sha256sum "$compiler" "$demo"/* "$result_dir"/input/*.ll "$result_dir"/native/*.o \
    "$result_dir"/units/raw/*.o "$result_dir"/units/unit-O2/*.o
} >"$result_dir/manifest.txt"

{
  echo '# Диспетчеризация 4П и оптимизации LLVM'
  echo
  echo "LLVM (opt): $opt_version. Все проверки выполнения пройдены."
  echo 'Сравниваются известная специализация, закрытое обобщение, открытый межмодульный набор и обычный C-указатель на функцию.'
  echo 'В extended только модуль empty заменяется модулем extension; остальные объектные файлы переиспользуются.'
  echo
  echo 'Статические числа инструкций и точек вызова не измеряют время выполнения.'
  echo
  awk -F '\t' '
    NR == 1 {
      print "| Набор / режим | Функция | Инструкции | alloca | load | store | Прямые вызовы | Косвенные вызовы |"
      print "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |"
      next
    }
    $2 ~ /^(probe_known|probe_closed|probe_open|probe_control|extra_probe)$/ {
      printf "| %s | %s | %s | %s | %s | %s | %s | %s |\n", $1, $2, $3, $4, $5, $6, $8, $9
    }
  ' "$result_dir/metrics.tsv"
  echo
  echo 'raw/unit-O2: program.ll — объединённая копия для анализа; запуск использует раздельные объектные файлы.'
  echo 'linked-O2: общий IR оптимизируется с сохранением внешней видимости символов.'
  echo 'internalized-O2: видимость ограничивается списком API из manifest.txt; все модули включаются до internalize.'
  echo 'Исходники, IR, ассемблер, результаты выполнения, pipeline и контрольные суммы сохранены рядом с отчётом.'
} >"$result_dir/report.md"
echo "LLVM dispatch demo passed; report: $result_dir/report.md"
