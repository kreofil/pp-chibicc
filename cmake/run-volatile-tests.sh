#!/bin/sh
set -eu
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
mkdir -p "$3"
result_dir=$(realpath "$3")
export PPCHIBICC_LLVM_OPT="$4" PPCHIBICC_LLVM_LLC="$5"
host_cc=$6
fail() { echo "Volatile tests: $*" >&2; exit 1; }

# An independent compiler validates ordinary C, beyond cross-backend agreement.
"$host_cc" -std=gnu11 -O2 "$source_dir/test/volatile.c" -o "$result_dir/reference"
"$result_dir/reference" > "$result_dir/reference.stdout"
cmp "$source_dir/test/llvm/valid/volatile.expected" "$result_dir/reference.stdout"

"$compiler" -S -emit-llvm "$source_dir/test/volatile.c" -o "$result_dir/volatile.ll"
"$PPCHIBICC_LLVM_OPT" -S -passes='default<O2>' -verify-each \
  "$result_dir/volatile.ll" -o "$result_dir/volatile.O2.ll"
python3 "$source_dir/cmake/check-volatile-ir.py" \
  "$result_dir/volatile.ll" "$result_dir/volatile.O2.ll"

for variant in native O0 O2; do
  set --
  case "$variant" in O0|O2) set -- --backend=llvm "-$variant" ;; esac
  for mode in runtime table switch; do
    prefix=$result_dir/ppc.$mode.$variant
    "$compiler" "$@" "--ppc-dispatch=$mode" "$source_dir/test/ppc-volatile.c" -o "$prefix.exe"
    "$prefix.exe" >"$prefix.stdout" 2>"$prefix.stderr" || fail "$mode $variant"
    cmp "$source_dir/test/llvm/valid/ppc-volatile.expected" "$prefix.stdout"
    [ ! -s "$prefix.stderr" ] || fail "$mode $variant stderr"
  done
done

echo 'Volatile mode checks passed'
