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

fail() {
  echo "LLVM optimization demo: $*" >&2
  exit 1
}

llvm_major() {
  "$1" --version | sed -n 's/.*LLVM version \([0-9][0-9]*\)\..*/\1/p' | head -n 1
}
major=$(llvm_major "$llvm_as")
[ -n "$major" ] || fail "cannot determine llvm-as version"
[ "$major" = "$(llvm_major "$llvm_opt")" ] &&
  [ "$major" = "$(llvm_major "$llvm_llc")" ] ||
  fail "llvm-as, opt and llc must have the same major version"
[ "$major" -ge 19 ] || fail "LLVM 19 or newer is required"
opt_version=$("$llvm_opt" --version | sed -n 's/.*LLVM version \([^[:space:]]*\).*/\1/p' | head -n 1)

source=$source_dir/examples/llvm/optimization.c
expected=$source_dir/examples/llvm/optimization.expected
cp "$source" "$result_dir/source.c"
cp "$expected" "$result_dir/expected.txt"

# All four pipelines start from exactly the same bitcode. Fix CPU, llc's
# optimization level and relocation model to isolate IR pass effects.
"$compiler" -S -emit-llvm "$source" -o "$result_dir/input.ll"
"$llvm_as" "$result_dir/input.ll" -o "$result_dir/input.bc"
"$llvm_opt" -passes=verify -disable-output "$result_dir/input.bc"

{
  date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
  echo "Source: $source"
  echo "Compiler: $compiler"
  echo "Host CC: $host_cc"
  echo "llvm-as: $llvm_as"
  echo "opt: $llvm_opt"
  echo "llc: $llvm_llc"
  echo 'Target: x86_64-pc-linux-gnu; CPU: x86-64; llc: -O=2; relocation: pic'
  echo 'Outputs: llc -filetype=asm for inspection; llc -filetype=obj for execution; host CC links objects'
  echo 'Inputs: 0 1 4 7 31; default: 4; rejected inputs: -1 33 (exit 2)'
  echo 'Host:'
  uname -srmo
  echo 'Checksums (compiler, source, expected output, input IR and bitcode):'
  sha256sum "$compiler" "$source" "$expected" "$result_dir/input.ll" "$result_dir/input.bc"
  if command -v git >/dev/null 2>&1 && git -C "$source_dir" rev-parse HEAD >/dev/null 2>&1; then
    echo "Repository HEAD: $(git -C "$source_dir" rev-parse HEAD)"
    if [ -n "$(git -C "$source_dir" status --porcelain)" ]; then
      echo 'Working tree: dirty (checksums identify actual inputs)'
    else
      echo 'Working tree: clean'
    fi
  fi
  echo 'Versions:'
  "$host_cc" --version | head -n 1
  "$llvm_as" --version
  "$llvm_opt" --version
  "$llvm_llc" --version
} >"$result_dir/manifest.txt"

check_execution() {
  execution_executable=$1
  execution_prefix=$2
  : >"$execution_prefix.stdout"
  : >"$execution_prefix.stderr"
  for count in 0 1 4 7 31; do
    "$execution_executable" "$count" >>"$execution_prefix.stdout" 2>>"$execution_prefix.stderr" ||
      fail "$execution_executable failed for n=$count"
  done
  cmp "$expected" "$execution_prefix.stdout" || fail "incorrect output: $execution_executable"
  [ ! -s "$execution_prefix.stderr" ] || fail "unexpected stderr: $execution_executable"
  "$execution_executable" >"$execution_prefix.default.stdout" 2>"$execution_prefix.default.stderr" ||
    fail "$execution_executable failed without arguments"
  awk '$1 == "n=4"' "$expected" >"$execution_prefix.default.expected"
  cmp "$execution_prefix.default.expected" "$execution_prefix.default.stdout" ||
    fail "incorrect default output: $execution_executable"
  [ ! -s "$execution_prefix.default.stderr" ] || fail "unexpected default stderr: $execution_executable"
  for count in -1 33; do
    if "$execution_executable" "$count" >"$execution_prefix.rejected.stdout" 2>"$execution_prefix.rejected.stderr"; then
      fail "$execution_executable accepted n=$count"
    else
      status=$?
    fi
    [ "$status" -eq 2 ] || fail "$execution_executable returned $status for n=$count"
    [ ! -s "$execution_prefix.rejected.stdout" ] && [ ! -s "$execution_prefix.rejected.stderr" ] ||
      fail "$execution_executable printed output for rejected n=$count"
  done
}

# Native execution is an additional reference, checked against fixed results.
"$compiler" -c "$source" -o "$result_dir/native.o"
"$host_cc" "$result_dir/native.o" -o "$result_dir/native.exe"
check_execution "$result_dir/native.exe" "$result_dir/native"

printf 'pipeline\tfunction\tinstructions\talloca\tload\tstore\tphi\tdirect_calls\tindirect_calls\tmemory_intrinsics\n' >"$result_dir/metrics.tsv"
printf 'pipeline\tpasses\topt_cpu\tllc_options\n' >"$result_dir/pipelines.tsv"

for variant in raw mem2reg sroa O2; do
  case $variant in
    raw) passes=verify ;;
    mem2reg) passes='function(mem2reg)' ;;
    sroa) passes='function(sroa,mem2reg)' ;;
    O2) passes='default<O2>' ;;
  esac
  prefix=$result_dir/$variant
  mkdir -p "$prefix"
  echo "LLVM $opt_version: $variant ($passes)"
  printf '%s\t%s\tx86-64\t-O=2 -mcpu=x86-64 -relocation-model=pic\n' \
    "$variant" "$passes" >>"$result_dir/pipelines.tsv"
  "$llvm_opt" -mcpu=x86-64 -passes="$passes" -print-pipeline-passes \
    -disable-output "$result_dir/input.bc" >"$prefix/passes.txt"
  "$llvm_opt" -mcpu=x86-64 -S -passes="$passes" -verify-each \
    "$result_dir/input.bc" -o "$prefix/program.ll"
  "$llvm_as" "$prefix/program.ll" -o "$prefix/program.bc"
  "$llvm_opt" -passes='print<func-properties>' -disable-output \
    "$prefix/program.bc" >"$prefix/properties.stdout" 2>"$prefix/properties.txt"
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=asm \
    "$prefix/program.bc" -o "$prefix/program.s"
  # A newer LLVM may print directives (e.g. .prefalign) that an older GNU as
  # cannot assemble. Use LLVM's object writer; keep .s as an inspection aid.
  "$llvm_llc" -O=2 -mcpu=x86-64 -relocation-model=pic -filetype=obj \
    "$prefix/program.bc" -o "$prefix/program.o"
  "$host_cc" "$prefix/program.o" -o "$prefix/program.exe"
  check_execution "$prefix/program.exe" "$prefix/result"
  awk -v pipeline="$variant" -f "$source_dir/cmake/llvm-ir-metrics.awk" \
    "$prefix/program.ll" >>"$result_dir/metrics.tsv"

  for function in grow local_sum dispatch_sum; do
    awk -v wanted="$function" '
      /^define / {
        name = $0
        sub(/^[^@]*@/, "", name)
        sub(/\(.*/, "", name)
        gsub(/"/, "", name)
        selected = name == wanted
      }
      selected { print }
      /^}/ { selected = 0 }
    ' "$prefix/program.ll" >"$prefix/$function.txt"
    [ -s "$prefix/$function.txt" ] || fail "missing function $function ($variant)"
  done
done

{
  echo '# Сравнение оптимизаций LLVM'
  echo
  echo "Версия LLVM (opt): $opt_version. Все проверки выполнения пройдены."
  echo 'Для каждого pipeline используются один исходный bitcode и одинаковые настройки llc.'
  echo
  echo 'Это статические числа инструкций IR; они не измеряют время выполнения или ускорение.'
  echo 'Direct calls не включают LLVM intrinsics; Memory intrinsics учитывают memset/memcpy/memmove.'
  echo
  awk -F '\t' '
    NR == 1 {
      print "| Pipeline | Function | Instructions | alloca | load | store | phi | Direct calls | Indirect calls | Memory intrinsics |"
      print "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"
      next
    }
    $2 == "MODULE" || $2 == "grow" || $2 == "local_sum" || $2 == "dispatch_sum" {
      printf "| %s | %s", $1, $2
      for (i = 3; i <= NF; i++) printf " | %s", $i
      print " |"
    }
  ' "$result_dir/metrics.tsv"
  echo
  echo 'Версии и контрольные суммы: manifest.txt. Точные параметры: pipelines.tsv.'
  echo 'Каталоги pipeline содержат IR, ассемблер, результаты выполнения и фрагменты функций.'
} >"$result_dir/report.md"

echo "LLVM optimization demo passed; report: $result_dir/report.md"
