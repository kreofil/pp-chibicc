#!/bin/sh
set -eu
source_dir=$1
compiler=$2
# $3 is the host C compiler, supplied by add_llvm_target.
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
result_dir=$7
llvm_link=$8
python=$9
export LC_ALL=C PYTHONDONTWRITEBYTECODE=1
exec "$python" "$source_dir/cmake/llvm-real-programs.py" \
  --source-dir "$source_dir" --compiler "$compiler" --result-dir "$result_dir" \
  --llvm-as "$llvm_as" --llvm-opt "$llvm_opt" --llvm-llc "$llvm_llc" --llvm-link "$llvm_link"
