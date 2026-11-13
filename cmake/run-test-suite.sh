#!/bin/sh
set -eu

source_dir=$(realpath "$1")
compiler=$(realpath "$2")
shift 2

cd "$source_dir"

for executable in "$@"; do
  echo "$executable"
  "$executable"
  echo
done

test/driver.sh "$compiler"

matrix_tmp=$(mktemp -d /tmp/ppchibicc-table-sizes-XXXXXX)
trap 'rm -rf "$matrix_tmp"' INT TERM HUP EXIT
"$source_dir/cmake/run-ppc-table-size-tests.sh" "$source_dir" "$compiler" "$matrix_tmp"
