#!/bin/sh
set -eu
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
host_cc=$3
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
mkdir -p "$7"
result_dir=$(realpath "$7")
export PPCHIBICC_LLVM_OPT=$llvm_opt PPCHIBICC_LLVM_LLC=$llvm_llc
source=$source_dir/test/llvm/valid/ppc-closed-dispatch.c
helper=$source_dir/test/llvm/support/closed-dispatch-override.c
expected=$source_dir/test/llvm/valid/ppc-closed-dispatch.expected
fail() { echo "LLVM closed dispatch: $*" >&2; exit 1; }
run() {
  exe=$1
  shift
  "$exe" "$@" >"$exe.stdout" 2>"$exe.stderr" || fail "execution: $exe $*"
  cmp "$expected" "$exe.stdout" || fail "output: $exe"
  [ ! -s "$exe.stderr" ] || fail "stderr: $exe"
}
"$compiler" --ppc-dispatch=runtime -c "$helper" -o "$result_dir/helper.native.o"
"$compiler" --backend=llvm --ppc-dispatch=runtime -O2 -c "$helper" -o "$result_dir/helper.llvm.o"
for mode in runtime table switch; do
  echo "testing closed dispatch $mode (native, LLVM O0/O2, overrides and reversed objects)"
  prefix=$result_dir/$mode
  "$compiler" "--ppc-dispatch=$mode" -S -emit-llvm "$source" -o "$prefix.ll"
  "$llvm_as" "$prefix.ll" -o "$prefix.bc"
  "$llvm_opt" -passes=verify -disable-output "$prefix.bc"
  if [ "$mode" = runtime ]; then
    if grep -q 'ppc_candidates' "$prefix.ll"; then fail 'runtime created a catalog'; fi
  else
    grep -Fq 'internal constant [3 x ptr]' "$prefix.ll" || fail 'missing immutable catalog'
    grep -Fq 'icmp eq ptr' "$prefix.ll" || fail 'missing active-handler guard'
    for name in dispatch_pick dispatch_case dispatch_pointer dispatch_union dispatch_late; do
      grep -Fq ".L.ppc_candidates.$name\" =" "$prefix.ll" || fail "missing catalog for $name"
    done
    for name in dispatch_incomplete dispatch_pair dispatch_mixed; do
      if grep -Fq ".L.ppc_candidates.$name\" =" "$prefix.ll"; then fail "ineligible $name was optimized"; fi
    done
    if [ "$mode" = switch ]; then
      grep -Fq 'switch i32' "$prefix.ll" || fail 'missing direct-call switch'
      grep -Fq 'call i32 @"__ppc_dispatch_pick_DispatchChoice_first"' "$prefix.ll" || fail 'missing direct handler call'
    fi
  fi
  "$compiler" "--ppc-dispatch=$mode" -c "$source" -o "$prefix.native.o"
  for level in 0 2; do
    "$compiler" --backend=llvm "--ppc-dispatch=$mode" "-O$level" -c "$source" -o "$prefix.O$level.o"
  done
  for variant in native O0 O2; do
    for other in native llvm; do
      "$host_cc" "$prefix.$variant.o" "$result_dir/helper.$other.o" -o "$prefix.$variant-$other.exe"
      run "$prefix.$variant-$other.exe"
      "$host_cc" "$result_dir/helper.$other.o" "$prefix.$variant.o" -o "$prefix.$variant-$other-reversed.exe"
      run "$prefix.$variant-$other-reversed.exe" reversed
    done
  done
done
echo 'LLVM closed dispatch tests passed'
