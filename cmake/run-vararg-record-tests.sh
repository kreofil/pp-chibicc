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
fail() { echo "Record varargs tests: $*" >&2; exit 1; }
index=0
reject() {
  index=$((index+1))
  cat >"$result_dir/invalid-$index.c"
  for backend in native llvm; do
    set -- -c
    [ "$backend" = native ] || set -- -S -emit-llvm
    status=0
    "$compiler" "$@" "$result_dir/invalid-$index.c" -o "$result_dir/invalid-$index.$backend.out" \
      >"$result_dir/invalid-$index.$backend.stdout" 2>"$result_dir/invalid-$index.$backend.stderr" || status=$?
    [ "$status" -eq 1 ] || fail "invalid $index ($backend): expected diagnostic, got $status"
    [ -s "$result_dir/invalid-$index.$backend.stderr" ] || fail "invalid $index ($backend): no diagnostic"
  done
}
for type in void 'int[2]' 'int[n]' 'int(void)' 'struct Incomplete'; do
  reject <<EOF
#include <stdarg.h>
struct Incomplete;
int f(int n,...) { va_list ap; va_start(ap,n); va_arg(ap,$type); return 0; }
EOF
done
for expression in 1 '(void *)0' bytes; do
  reject <<EOF
#include <stdarg.h>
struct S { int n; };
int f(int n,...) { char bytes[24]; va_arg($expression,struct S); return 0; }
EOF
done
reject <<'EOF'
struct S { int n; };
struct S f(void *p) { return __builtin_va_arg(p,struct S); }
EOF
check() {
  prefix=$1 expected=$2
  "$prefix.exe" >"$prefix.stdout" 2>"$prefix.stderr" || fail "$prefix execution"
  printf '%s\n' "$expected" >"$result_dir/expected"
  cmp "$result_dir/expected" "$prefix.stdout" || fail "$prefix output"
  [ ! -s "$prefix.stderr" ] || fail "$prefix stderr"
}
for mode in runtime table switch; do
  dir=$result_dir/$mode
  mkdir -p "$dir"
  for variant in $variants; do
    set --
    case "$variant" in O0|O2) set -- --backend=llvm "-$variant" ;; esac
    "$compiler" "$@" "--ppc-dispatch=$mode" "$source_dir/test/ppc-vararg-records.c" -o "$dir/ppc.$variant.exe"
    check "$dir/ppc.$variant" 'PPC record varargs OK'
    for unit in base spec main; do
      "$compiler" "$@" "--ppc-dispatch=$mode" -c \
        "$source_dir/test/llvm/support/vararg-record-$unit.c" -o "$dir/$unit.$variant.o"
    done
    "$compiler" "$dir/base.$variant.o" "$dir/spec.$variant.o" "$dir/main.$variant.o" -o "$dir/all.$variant.exe"
    check "$dir/all.$variant" 'mixed record varargs OK'
  done
  case " $variants " in *' O2 '*)
    for singled in native O2; do
      for unit in base spec main; do
        set --
        for other in base spec main; do
          variant=native
          [ "$singled" != native ] || variant=O2
          [ "$unit" != "$other" ] || variant=$singled
          set -- "$@" "$dir/$other.$variant.o"
        done
        prefix=$dir/mixed-$unit-$singled
        "$compiler" "$@" -o "$prefix.exe"
        check "$prefix" 'mixed record varargs OK'
      done
    done
  esac
done
echo 'Record varargs mode and mixed-object checks passed'
