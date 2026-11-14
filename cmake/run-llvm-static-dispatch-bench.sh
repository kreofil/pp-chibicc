#!/bin/sh
set -eu
export LC_ALL=C
export PYTHONDONTWRITEBYTECODE=1
[ "$#" -eq 9 ] || [ "$#" -eq 10 ] || { echo 'LLVM static benchmark: expected build tools, Python and optional --check-only' >&2; exit 1; }
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
shift 9
exec "$python" "$source_dir/cmake/llvm-static-dispatch-bench.py" \
  --source-dir "$source_dir" --compiler "$compiler" --host-cc "$host_cc" \
  --llvm-as "$llvm_as" --llvm-opt "$llvm_opt" --llvm-llc "$llvm_llc" \
  --llvm-link "$llvm_link" --result-dir "$result_dir" "$@"
