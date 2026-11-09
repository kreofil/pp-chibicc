#!/bin/sh
set -eu

source_dir=$1
compiler=$(realpath "$2")
shift 2

cd "$source_dir"

for executable in "$@"; do
  echo "$executable"
  "$executable"
  echo
done

test/driver.sh "$compiler"
