#!/bin/sh
set -eu
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
result_dir=$7
llvm_link=$8
python=$9
shift 9
export LC_ALL=C PYTHONDONTWRITEBYTECODE=1
exec "$python" "$source_dir/cmake/llvm-life-bench.py" \
  --source-dir "$source_dir" --compiler "$compiler" --llvm-as "$llvm_as" \
  --llvm-opt "$llvm_opt" --llvm-llc "$llvm_llc" --llvm-link "$llvm_link" \
  --result-dir "$result_dir" "$@"
