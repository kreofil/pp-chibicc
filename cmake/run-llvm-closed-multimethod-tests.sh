#!/bin/sh
set -eu
ulimit -c 0
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
host_cc=$3
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
mkdir -p "$7"
result_dir=$(realpath "$7")
export PPCHIBICC_LLVM_OPT="$llvm_opt" PPCHIBICC_LLVM_LLC="$llvm_llc"
source=$source_dir/test/llvm/valid/ppc-closed-multimethod.c
helper=$source_dir/test/llvm/support/closed-multimethod-override.c
opaque=$source_dir/test/llvm/support/closed-multimethod-opaque.c
expected=$source_dir/test/llvm/valid/ppc-closed-multimethod.expected
fail() { echo "LLVM closed multimethod: $*" >&2; exit 1; }
for backend in native llvm; do
  set --
  [ "$backend" = native ] || set -- --backend=llvm -O2
  "$compiler" "$@" --ppc-dispatch=runtime -c "$helper" -o "$result_dir/helper.$backend.o"
  "$compiler" "$@" --ppc-dispatch=runtime -c "$opaque" -o "$result_dir/opaque.$backend.o"
  "$compiler" "$@" --ppc-dispatch=runtime -DOMIT_REMOTE_DEFAULT -c "$helper" -o "$result_dir/missing.$backend.o"
done
for mode in runtime table switch; do
  echo "testing closed multimethod $mode (2/3 arguments, defaults and all object orders)"
  prefix=$result_dir/$mode
  "$compiler" "--ppc-dispatch=$mode" -S -emit-llvm "$source" -o "$prefix.ll"
  "$llvm_as" "$prefix.ll" -o "$prefix.bc"
  "$llvm_opt" -passes=verify -disable-output "$prefix.bc"
  if [ "$mode" = runtime ]; then
    if grep -q 'ppc_candidates' "$prefix.ll"; then fail 'runtime created a catalog'; fi
  else
    for name in multi_early multi_late; do
      grep -Fq ".L.ppc_candidates.$name\" = internal constant [6 x ptr]" "$prefix.ll" || fail "missing 2D catalog $name"
    done
    for name in multi_triple multi_small multi_varargs; do
      grep -Fq ".L.ppc_candidates.$name\" = internal constant [12 x ptr]" "$prefix.ll" || fail "missing 3D catalog $name"
    done
    grep -Fq '.L.ppc_candidates.multi_union" = internal constant [4 x ptr]' "$prefix.ll" || fail 'union catalog missing'
    for name in multi_remote multi_open_left multi_open_right multi_large_grid; do
      if grep -Fq ".L.ppc_candidates.$name\" =" "$prefix.ll"; then fail "ineligible $name optimized"; fi
    done
  fi
  "$compiler" "--ppc-dispatch=$mode" -c "$source" -o "$prefix.native.o"
  "$compiler" "--ppc-dispatch=$mode" "$source_dir/test/ppc-multimethod-vla.c" -o "$prefix.vla.exe"
  "$prefix.vla.exe" >"$prefix.vla.stdout"
  grep -qx 'multimethod VLA OK' "$prefix.vla.stdout" || fail 'native VLA argument decay'
  for level in 0 2; do
    "$compiler" --backend=llvm "--ppc-dispatch=$mode" "-O$level" \
      "$source_dir/test/ppc-multimethod-vla.c" -o "$prefix.vla.O$level.exe"
    "$prefix.vla.O$level.exe" >"$prefix.vla.O$level.stdout" 2>"$prefix.vla.O$level.stderr"
    cmp "$prefix.vla.stdout" "$prefix.vla.O$level.stdout" || fail 'LLVM VLA argument decay'
    [ ! -s "$prefix.vla.O$level.stderr" ] || fail 'LLVM VLA argument stderr'
    "$compiler" --backend=llvm "--ppc-dispatch=$mode" "-O$level" -c "$source" -o "$prefix.O$level.o"
  done
  for variant in native O0 O2; do
    for other in native llvm; do
      for order in phq pqh hpq hqp qph qhp; do
        set --
        for unit in $(printf '%s' "$order" | sed 's/./& /g'); do
          case "$unit" in
            p) set -- "$@" "$prefix.$variant.o" ;;
            h) set -- "$@" "$result_dir/helper.$other.o" ;;
            q) set -- "$@" "$result_dir/opaque.$other.o" ;;
          esac
        done
        exe=$prefix.$variant-$other-$order.exe
        "$host_cc" "$@" -o "$exe"
        "$exe" >"$exe.stdout" 2>"$exe.stderr" || fail "execution $exe"
        cmp "$expected" "$exe.stdout" || fail "output $exe"
        [ ! -s "$exe.stderr" ] || fail "stderr $exe"
      done
    done
  done
done
# An opaque unit's error stub must not hide a real closed-family hole.
for other in native llvm; do
  for order in phq pqh hpq hqp qph qhp; do
    set --
    for unit in $(printf '%s' "$order" | sed 's/./& /g'); do
      case "$unit" in
        p) set -- "$@" "$result_dir/switch.O2.o" ;;
        h) set -- "$@" "$result_dir/missing.$other.o" ;;
        q) set -- "$@" "$result_dir/opaque.$other.o" ;;
      esac
    done
    exe=$result_dir/missing-$other-$order.exe
    "$host_cc" "$@" -o "$exe"
    if "$exe" >"$exe.stdout" 2>"$exe.stderr"; then fail 'missing default reached main'; else status=$?; fi
    [ "$status" -eq 134 ] || fail "missing default failed with $status"
    [ ! -s "$exe.stdout" ] || fail 'missing default produced main output'
    grep -Fq 'PPC completeness error: missing handler for multi_remote table slot 0' "$exe.stderr" || fail 'missing pre-main completeness diagnostic'
  done
done
echo 'LLVM closed multimethod tests passed'
