#!/bin/sh
set -eu
source_dir=$1
compiler=$2
host_cc=$3
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
result_dir=$7
llvm_link=$8
python=$9
cmake=${10}
install_build=${11}
export LC_ALL=C PYTHONDONTWRITEBYTECODE=1
exec "$python" "$source_dir/cmake/test-llvm-closed-compiler.py" \
  --source-dir "$source_dir" --compiler "$compiler" --host-cc "$host_cc" \
  --llvm-as "$llvm_as" --llvm-opt "$llvm_opt" --llvm-llc "$llvm_llc" \
  --llvm-link "$llvm_link" --result-dir "$result_dir" --cmake "$cmake" \
  --install-build "$install_build"
