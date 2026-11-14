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
llvm_link=$8
python=$9
exec "$python" "$source_dir/cmake/test-ppc-static-dispatch.py" \
  --source-dir "$source_dir" --compiler "$compiler" --host-cc "$host_cc" \
  --llvm-as "$llvm_as" --llvm-opt "$llvm_opt" --llvm-llc "$llvm_llc" \
  --llvm-link "$llvm_link" --result-dir "$result_dir"
