#!/bin/sh
set -eu
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
mkdir -p "$3"
result_dir=$(realpath "$3")
variants=native
if [ "$#" -ge 5 ]; then
  export PPCHIBICC_LLVM_OPT="$4" PPCHIBICC_LLVM_LLC="$5"
  variants='native O0 O2'
fi
fail() { echo "Bitfield tests: $*" >&2; exit 1; }
index=0
for source in \
  'struct S { int x:-1; };' \
  'struct S { unsigned x:33; };' \
  'struct S { unsigned long x:65; };' \
  'struct S { _Bool x:2; };' \
  'struct S { int x:0; };' \
  'struct S { float x:3; };' \
  'struct S { int *x:3; };' \
  'struct S { _Atomic int x:3; };' \
  'struct S { _Alignas(8) int x:3; };' \
  'struct S { int x:1.5; };' \
  'int n; struct S { int x:n; };' \
  'struct S {} case { unsigned code:33; } const;' \
  'struct S {} case { default _Bool flag:2; } const;' \
  'struct S { int x:3; } s; int *p=&s.x;' \
  'struct S { int x:3; } s; int n=sizeof(s.x);' \
  'struct S { int x:3; } s; int n=_Alignof(s.x);' \
  'struct S { int x:3; } s; typeof(s.x) n;'; do
  index=$((index+1))
  prefix=$result_dir/invalid-$index
  printf '%s\n' "$source" >"$prefix.c"
  for backend in native llvm; do
    set -- -c
    [ "$backend" = native ] || set -- -S -emit-llvm
    status=0
    "$compiler" "$@" "$prefix.c" -o "$prefix.$backend.out" \
      >"$prefix.$backend.stdout" 2>"$prefix.$backend.stderr" || status=$?
    [ "$status" -eq 1 ] || fail "invalid $index ($backend): expected diagnostic, got $status"
    [ -s "$prefix.$backend.stderr" ] || fail "invalid $index ($backend): no diagnostic"
  done
done
for variant in $variants; do
  set --
  case "$variant" in O0|O2) set -- --backend=llvm "-$variant" ;; esac
  prefix=$result_dir/guard.$variant
  "$compiler" "$@" "$source_dir/test/llvm/support/bitfield-guard.c" -o "$prefix.exe"
  "$prefix.exe" >"$prefix.stdout" 2>"$prefix.stderr" || fail "guard $variant"
  printf 'bitfield guard OK\n' >"$result_dir/guard.expected"
  cmp "$result_dir/guard.expected" "$prefix.stdout" || fail "guard $variant output"
  [ ! -s "$prefix.stderr" ] || fail "guard $variant stderr"
  for mode in runtime table switch; do
    prefix=$result_dir/ppc.$mode.$variant
    "$compiler" "$@" "--ppc-dispatch=$mode" "$source_dir/test/ppc-bitfield.c" -o "$prefix.exe"
    "$prefix.exe" >"$prefix.stdout" 2>"$prefix.stderr" || fail "PPC $mode $variant"
    cmp "$source_dir/test/llvm/valid/ppc-bitfield.expected" "$prefix.stdout" || fail "PPC $mode $variant output"
    [ ! -s "$prefix.stderr" ] || fail "PPC $mode $variant stderr"
  done
done
echo 'Bitfield checks passed'
