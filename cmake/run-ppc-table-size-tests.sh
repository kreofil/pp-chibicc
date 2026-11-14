#!/bin/sh
set -eu
ulimit -c 0
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
mkdir -p "$3"
result_dir=$(realpath "$3")
variants=native
if [ "$#" -eq 5 ]; then
  export PPCHIBICC_LLVM_OPT="$4" PPCHIBICC_LLVM_LLC="$5"
  variants='native O0 O2'
fi
source=$source_dir/test/ppc-table-size.c
fail() { echo "PPC table dimensions: $*" >&2; exit 1; }
for variant in $variants; do
  for openness in closed open; do
    set --
    [ "$variant" = native ] || set -- --backend=llvm "-$variant"
    [ "$openness" = closed ] || set -- "$@" -DTEST_OPEN
    prefix=$result_dir/$variant-$openness
    "$compiler" "$@" "$source" -o "$prefix.exe"
    "$prefix.exe" >"$prefix.stdout" 2>"$prefix.stderr" || fail "positive $variant/$openness"
    grep -qx 'table dimensions OK' "$prefix.stdout" || fail 'positive result'
    [ ! -s "$prefix.stderr" ] || fail 'positive stderr'
    for kind in TEST_HUGE TEST_FAIL_ALLOC; do
      exe=$prefix-$kind.exe
      "$compiler" "$@" "-D$kind" "$source" -o "$exe"
      if (ulimit -v 131072 || exit 99; set +e; "$exe"; exit $?) >"$exe.stdout" 2>"$exe.stderr"; then
        fail "$kind reached main"
      else
        status=$?
      fi
      [ "$status" -eq 134 ] || fail "$kind exited with $status"
      [ ! -s "$exe.stdout" ] || fail "$kind produced main output"
      slots=4294967296
      [ "$kind" = TEST_HUGE ] || slots=4
      grep -qx "PPC table error: allocation failed for huge (slots $slots)" "$exe.stderr" || fail 'allocation diagnostic'
    done
    for overflow_kind in TEST_BYTE_OVERFLOW TEST_OVERFLOW; do
      exe=$prefix-$overflow_kind.exe
      if [ "$openness" = closed ]; then
        if "$compiler" "$@" "-D$overflow_kind" -c "$source" -o "$exe.o" >"$exe.stdout" 2>"$exe.stderr"; then
          fail 'closed oversized matrix compiled'
        fi
        grep -Fq 'PPC table size overflows for huge' "$exe.stderr" || fail 'closed overflow diagnostic'
      else
        "$compiler" "$@" "-D$overflow_kind" "$source" -o "$exe"
        if (ulimit -v 131072 || exit 99; set +e; "$exe"; exit $?) >"$exe.stdout" 2>"$exe.stderr"; then
          fail 'open oversized matrix reached main'
        else
          status=$?
        fi
        [ "$status" -eq 134 ] || fail "open overflow exited with $status"
        [ ! -s "$exe.stdout" ] || fail 'open overflow produced main output'
        grep -qx 'PPC table error: size overflow for huge' "$exe.stderr" || fail 'open overflow diagnostic'
      fi
    done
  done
done
echo 'PPC table dimension checks passed'
