#!/bin/sh
set -eu

source_dir=$1
compiler=$(realpath "$2")
tmp=$(mktemp -d /tmp/ppchibicc-examples-XXXXXX)
trap 'rm -rf "$tmp"' INT TERM HUP EXIT

cd "$source_dir"

steps='01-start 02-new-spec 03-new-func 04-new-field 05-rect-only-out 06-mm 07-mm-new-spec 08-more-gen'
base=examples/evolution/figures/evo-reuse-code

for name in $steps; do
  dir=$base/$name
  srcdir=$dir/src
  out=$tmp/$name
  result=$tmp/$name.out

  echo "testing $dir"

  files=$(awk '/set\(SOURCE_FILES/{flag=1; next} /^\)/{flag=0} flag {print}' \
      "$srcdir/CMakeLists.txt" |
    sed 's/#.*//' |
    tr -s '[:space:]' '\n' |
    grep '\.c$' |
    sed "s|^|$srcdir/|")

  incs="-I$source_dir/include"
  for include_dir in "$source_dir"/examples/evolution/figures/evo-reuse-code/*/src; do
    incs="$incs -I$include_dir"
  done

  # Word splitting is intentional for the compiler argument lists assembled
  # from the example CMake files above.
  "$compiler" $incs -o "$out" $files
  "$out" "$dir/data/input1.txt" "$result" >"$tmp/$name.stdout"
  diff -u "$dir/data/output1.txt" "$result"
done
