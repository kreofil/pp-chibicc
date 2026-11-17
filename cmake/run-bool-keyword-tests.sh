#!/bin/sh
set -eu
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
mkdir -p "$3"
result_dir=$(realpath "$3")
variants=native
host_cc=
if [ "$#" -ge 6 ]; then
  host_cc=$6
fi
if [ "$#" -ge 5 ]; then
  export PPCHIBICC_LLVM_OPT="$4" PPCHIBICC_LLVM_LLC="$5"
  variants='native O0 O2'
fi
fail() { echo "Boolean keyword tests: $*" >&2; exit 1; }

index=0
for source in \
  'bool int value;' \
  'unsigned bool value;' \
  'int bool;' \
  'int true;' \
  'int false;' \
  'enum E { true };' \
  'struct S { int false; };' \
  'int main(void) { true = false; return 0; }' \
  'int main(void) { return *true; }' \
  'int main(void) { return true++; }' \
  'int main(void) { int *p = &false; return 0; }'; do
  index=$((index + 1))
  prefix=$result_dir/invalid-$index
  printf '%s\n' "$source" >"$prefix.c"
  for backend in native llvm; do
    set -- -c
    [ "$backend" = native ] || set -- -S -emit-llvm
    status=0
    "$compiler" "$@" "$prefix.c" -o "$prefix.$backend.out" \
      >"$prefix.$backend.stdout" 2>"$prefix.$backend.stderr" || status=$?
    [ "$status" -eq 1 ] || fail "invalid $index ($backend): expected diagnostic exit 1, got $status"
    [ -s "$prefix.$backend.stderr" ] || fail "invalid $index ($backend): missing diagnostic"
    [ ! -s "$prefix.$backend.stdout" ] || fail "invalid $index ($backend): unexpected stdout"
  done
done

for mode in runtime table switch; do
  for variant in $variants; do
    set --
    case "$variant" in O0|O2) set -- --backend=llvm "-$variant" ;; esac
    exe=$result_dir/$mode.$variant.exe
    "$compiler" "$@" "--ppc-dispatch=$mode" "$source_dir/test/ppc-bool.c" -o "$exe"
    "$exe" >"$exe.stdout" 2>"$exe.stderr" || fail "$mode $variant execution"
    cmp "$source_dir/test/llvm/valid/ppc-bool.expected" "$exe.stdout" || fail "$mode $variant output"
    [ ! -s "$exe.stderr" ] || fail "$mode $variant stderr"
  done
done
# The public header keeps legacy macros for foreign C11/C17 compilers, and
# preserves real keywords when those compilers select their C23 mode.
if [ -n "$host_cc" ]; then
  cat >"$result_dir/header.c" <<'EOF'
#include <stdbool.h>
#if __STDC_VERSION__ > 201710L
#if defined(bool) || defined(true) || defined(false)
#error C23 Boolean words must not be macros
#endif
enum { LiteralTypeOK = _Generic(true, _Bool: 1, default: 0) };
#else
#if !defined(bool) || !defined(true) || !defined(false)
#error Legacy Boolean macros are missing
#endif
enum { LiteralTypeOK = _Generic(true, int: 1, default: 0) };
#endif
int main(void) { bool value = true; return !value || false || !LiteralTypeOK; }
EOF
  for standard in c11 c17 c2x; do
    "$host_cc" "-std=$standard" "-I$source_dir/include" "$result_dir/header.c" \
      -o "$result_dir/header-$standard.exe"
    "$result_dir/header-$standard.exe" >"$result_dir/header-$standard.stdout" \
      2>"$result_dir/header-$standard.stderr" || fail "foreign header ($standard) execution"
    [ ! -s "$result_dir/header-$standard.stdout" ] || fail "foreign header ($standard) stdout"
    [ ! -s "$result_dir/header-$standard.stderr" ] || fail "foreign header ($standard) stderr"
  done
fi
echo 'Boolean keyword checks passed'
