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

fail() {
  echo "LLVM test failure: $*" >&2
  exit 1
}

# Avoid accidentally assembling with one LLVM release and optimizing with
# another. The compiler itself does not need any of these tools installed.
llvm_major() {
  "$1" --version | sed -n 's/.*LLVM version \([0-9][0-9]*\)\..*/\1/p' | head -n 1
}
as_major=$(llvm_major "$llvm_as")
opt_major=$(llvm_major "$llvm_opt")
llc_major=$(llvm_major "$llvm_llc")
[ -n "$as_major" ] || fail "cannot determine llvm-as version"
[ "$as_major" = "$opt_major" ] && [ "$as_major" = "$llc_major" ] ||
  fail "llvm-as, opt and llc must have the same major version"
[ "$as_major" -ge 19 ] || fail "LLVM 19 or newer is required"
echo "LLVM tests: LLVM $as_major; artifacts in $result_dir"

cd "$source_dir"

check_execution() {
  executable=$1
  output_prefix=$2
  expected_output=$3
  expected_status=$4
  if "$executable" >"$output_prefix.stdout" 2>"$output_prefix.stderr"; then
    actual_status=0
  else
    actual_status=$?
  fi
  [ "$actual_status" -eq "$expected_status" ] ||
    fail "$executable exited with $actual_status; expected $expected_status"
  diff -u "$expected_output" "$output_prefix.stdout" ||
    fail "unexpected output from $executable"
  [ ! -s "$output_prefix.stderr" ] || fail "unexpected stderr from $executable"
}

for source in "$source_dir"/test/llvm/valid/*.c \
    "$source_dir"/examples/llvm/ppc-figures.c "$source_dir"/examples/llvm/ppc-by-value.c \
    "$source_dir"/examples/llvm/ppc-floating.c \
    "$source_dir"/examples/llvm/ppc-floating-by-value.c \
    "$source_dir"/examples/llvm/ppc-vla-array.c \
    "$source_dir"/examples/llvm/ppc-vla-matrix.c \
    "$source_dir"/examples/llvm/ppc-bool-flags.c \
    "$source_dir"/examples/llvm/ppc-bitfields.c \
    "$source_dir"/examples/llvm/ppc-vararg-items.c; do
  name=$(basename "$source" .c)
  prefix=$result_dir/$name
  expected_status=0
  if [ -f "$source_dir/test/llvm/valid/$name.status" ]; then
    expected_status=$(cat "$source_dir/test/llvm/valid/$name.status")
  fi
  echo "testing LLVM $name (x86, LLVM O0, LLVM O2)"

  # ABI fixtures link against a separately host-compiled translation unit.
  # The helper calls emitted functions as well as being called by them.
  set --
  if [ "$name" = abi ]; then
    "$host_cc" -std=c11 -c "$source_dir/test/llvm/support/abi-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = bool-keywords ]; then
    "$host_cc" -std=c11 -O2 -c "$source_dir/test/llvm/support/bool-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = records ]; then
    "$host_cc" -std=c11 -c "$source_dir/test/llvm/support/record-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = record-abi ]; then
    "$host_cc" -std=c11 -O2 -c "$source_dir/test/llvm/support/record-abi-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = bitfield-abi ]; then
    "$host_cc" -std=c11 -O2 -c "$source_dir/test/llvm/support/bitfield-abi-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = floating-abi ]; then
    "$host_cc" -std=c11 -O2 -c "$source_dir/test/llvm/support/floating-abi-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = empty-params ]; then
    "$host_cc" -std=c11 -c "$source_dir/test/llvm/support/empty-params-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = stdarg-abi ]; then
    "$host_cc" -std=c11 -c "$source_dir/test/llvm/support/stdarg-abi-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = stdarg-records ]; then
    "$host_cc" -std=c11 -O2 -c "$source_dir/test/llvm/support/stdarg-record-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = ppc-closed-multimethod ]; then
    "$compiler" --ppc-dispatch=runtime -c "$source_dir/test/llvm/support/closed-multimethod-override.c" \
      -o "$prefix.override.o"
    "$compiler" --ppc-dispatch=runtime -c "$source_dir/test/llvm/support/closed-multimethod-opaque.c" \
      -o "$prefix.opaque.o"
    set -- "$prefix.override.o" "$prefix.opaque.o"
  elif [ "$name" = ppc-closed-dispatch ]; then
    "$compiler" --ppc-dispatch=runtime -c "$source_dir/test/llvm/support/closed-dispatch-override.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = ppc-closed-indices ]; then
    "$host_cc" -std=c11 -c "$source_dir/test/llvm/support/closed-init-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = long-double-abi ]; then
    "$host_cc" -std=c11 -c "$source_dir/test/llvm/support/long-double-abi-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = vla ] || [ "$name" = ppc-vla ] || [ "$name" = bitfield ]; then
    "$host_cc" -std=c11 -c "$source_dir/test/llvm/support/assert-quiet.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  elif [ "$name" = vla-params ]; then
    "$host_cc" -std=c11 -O2 -c "$source_dir/test/llvm/support/vla-params-host.c" \
      -o "$prefix.host.o"
    set -- "$prefix.host.o"
  fi

  # Use the system compiler as an independent reference for ABI boundaries
  # and IEEE floating-point edge cases. The native backend has existing
  # limitations here (notably NaN conditions and unsigned 64-bit conversions).
  if [ "$name" = bool-keywords ]; then
    "$host_cc" -std=c2x -O2 -c "$source" -o "$prefix.x86.o"
  elif [ "$name" = bitfield-abi ] || [ "$name" = record-abi ] || [ "$name" = floating-abi ] || [ "$name" = floating ] || [ "$name" = switch ] || [ "$name" = empty-params ] || [ "$name" = stdarg-abi ] || [ "$name" = stdarg-records ] || [ "$name" = long-double ] || [ "$name" = long-double-abi ] || [ "$name" = vla-params ]; then
    "$host_cc" -std=c11 -O2 -c "$source" -o "$prefix.x86.o"
  else
    "$compiler" -c "$source" -o "$prefix.x86.o"
  fi
  "$host_cc" "$prefix.x86.o" "$@" -o "$prefix.x86.exe"
  check_execution "$prefix.x86.exe" "$prefix.x86" \
    "$source_dir/test/llvm/valid/$name.expected" "$expected_status"

  "$compiler" -S -emit-llvm "$source" -o "$prefix.ll"
  "$llvm_as" "$prefix.ll" -o "$prefix.bc"
  "$llvm_opt" -passes=verify -disable-output "$prefix.bc"
  "$llvm_llc" -O=0 -filetype=obj -relocation-model=pic "$prefix.bc" \
    -o "$prefix.O0.o"
  "$host_cc" "$prefix.O0.o" "$@" -o "$prefix.O0.exe"
  check_execution "$prefix.O0.exe" "$prefix.O0" \
    "$prefix.x86.stdout" "$expected_status"

  "$llvm_opt" -S -passes='default<O2>' -verify-each "$prefix.bc" \
    -o "$prefix.O2.ll"
  "$llvm_llc" -O=2 -filetype=obj -relocation-model=pic "$prefix.O2.ll" \
    -o "$prefix.O2.o"
  "$host_cc" "$prefix.O2.o" "$@" -o "$prefix.O2.exe"
  check_execution "$prefix.O2.exe" "$prefix.O2" \
    "$prefix.x86.stdout" "$expected_status"

  # Exercise the public driver and its own linker with the same independent
  # ABI/IEEE reference, not just the manually orchestrated LLVM pipeline.
  for level in 0 2; do
    PPCHIBICC_LLVM_OPT="$llvm_opt" PPCHIBICC_LLVM_LLC="$llvm_llc" \
      "$compiler" --backend=llvm "-O$level" "$source" "$@" -o "$prefix.driver-O$level.exe"
    check_execution "$prefix.driver-O$level.exe" "$prefix.driver-O$level" \
      "$prefix.x86.stdout" "$expected_status"
  done
done

"$source_dir/cmake/run-vla-parameter-tests.sh" "$source_dir" "$compiler" \
  "$result_dir/vla-params-modes" "$llvm_opt" "$llvm_llc"
"$source_dir/cmake/run-bool-keyword-tests.sh" "$source_dir" "$compiler" \
  "$result_dir/bool-keywords-modes" "$llvm_opt" "$llvm_llc" "$host_cc"
"$source_dir/cmake/run-bitfield-tests.sh" "$source_dir" "$compiler" \
  "$result_dir/bitfields-modes" "$llvm_opt" "$llvm_llc"
"$source_dir/cmake/run-vararg-record-tests.sh" "$source_dir" "$compiler" \
  "$result_dir/record-varargs-modes" "$llvm_opt" "$llvm_llc"

# VLA constraints are shared frontend diagnostics in both backends.
for statement in 'int a[n], b[n]; a = b;' 'int a[n] = {1};'; do
  printf 'int main(void) { int n = 3; %s return 0; }\n' "$statement" >"$result_dir/vla-invalid.c"
  for backend in native llvm; do
    set -- -c
    [ "$backend" = native ] || set -- -S -emit-llvm
    if "$compiler" "$@" "$result_dir/vla-invalid.c" -o "$result_dir/vla-invalid.o" \
        >"$result_dir/vla-invalid.stdout" 2>"$result_dir/vla-invalid.stderr"; then
      fail "invalid VLA accepted ($backend): $statement"
    fi
    grep -Eq 'not an lvalue|variable-sized object may not be initialized' \
      "$result_dir/vla-invalid.stderr" || fail "missing VLA constraint diagnostic ($backend)"
  done
done

# Each unit keeps the same common symbols and registration phases across
# object files. Test both homogeneous builds and mixed native/LLVM objects.
echo "testing LLVM PPC multifile (x86, O0, O2, mixed objects)"
multifile=$result_dir/multifile
mkdir -p "$multifile"
for unit in base rectangle square opaque main; do
  source=$source_dir/test/llvm/multifile/$unit.c
  prefix=$multifile/$unit
  "$compiler" -c "$source" -o "$prefix.x86.o"
  "$compiler" -S -emit-llvm "$source" -o "$prefix.ll"
  "$llvm_as" "$prefix.ll" -o "$prefix.bc"
  "$llvm_opt" -passes=verify -disable-output "$prefix.bc"
  "$llvm_llc" -O=0 -filetype=obj -relocation-model=pic "$prefix.bc" -o "$prefix.O0.o"
  "$llvm_opt" -S -passes='default<O2>' -verify-each "$prefix.bc" -o "$prefix.O2.ll"
  "$llvm_llc" -O=2 -filetype=obj -relocation-model=pic "$prefix.O2.ll" -o "$prefix.O2.o"
done
for variant in x86 O0 O2; do
  "$host_cc" "$multifile/base.$variant.o" "$multifile/rectangle.$variant.o" \
    "$multifile/square.$variant.o" "$multifile/opaque.$variant.o" "$multifile/main.$variant.o" -o "$multifile/$variant.exe"
  check_execution "$multifile/$variant.exe" "$multifile/$variant" \
    "$source_dir/test/llvm/multifile/expected.txt" 0
  "$host_cc" "$multifile/main.$variant.o" "$multifile/opaque.$variant.o" \
    "$multifile/square.$variant.o" "$multifile/rectangle.$variant.o" \
    "$multifile/base.$variant.o" -o "$multifile/$variant-reversed.exe"
  check_execution "$multifile/$variant-reversed.exe" "$multifile/$variant-reversed" \
    "$source_dir/test/llvm/multifile/expected.txt" 0
done
for native in base rectangle square opaque main; do
  set --
  for unit in base rectangle square opaque main; do
    variant=O2
    [ "$unit" != "$native" ] || variant=x86
    set -- "$@" "$multifile/$unit.$variant.o"
  done
  "$host_cc" "$@" -o "$multifile/mixed-$native.exe"
  check_execution "$multifile/mixed-$native.exe" "$multifile/mixed-$native" \
    "$source_dir/test/llvm/multifile/expected.txt" 0
done

# Abort diagnostics exercise runtime-generated variadic dprintf calls and
# distinguish an uncovered open family from a failed closed-family startup.
ulimit -c 0
for feature in UNCOVERED UNCOVERED_MULTI UNCOVERED_FLOAT UNCOVERED_RECORD UNCOVERED_REAL_RECORD INCOMPLETE; do
  echo "testing LLVM PPC runtime diagnostic $feature"
  prefix=$result_dir/runtime-$feature
  "$compiler" -c -DFAIL_$feature "$source_dir/test/llvm/runtime-fail.c" -o "$prefix.x86.o"
  "$compiler" -S -emit-llvm -DFAIL_$feature "$source_dir/test/llvm/runtime-fail.c" -o "$prefix.ll"
  "$llvm_as" "$prefix.ll" -o "$prefix.bc"
  "$llvm_llc" -O=0 -filetype=obj -relocation-model=pic "$prefix.bc" -o "$prefix.O0.o"
  "$llvm_opt" -S -passes='default<O2>' -verify-each "$prefix.bc" -o "$prefix.O2.ll"
  "$llvm_llc" -O=2 -filetype=obj -relocation-model=pic "$prefix.O2.ll" -o "$prefix.O2.o"
  case $feature in
    UNCOVERED) message='PPC dispatch error: uncovered handler for dispatch<' ;;
    UNCOVERED_MULTI) message='PPC dispatch error: uncovered handler for dispatch<1,' ;;
    UNCOVERED_FLOAT) message='PPC dispatch error: uncovered handler for dispatch<1>()' ;;
    UNCOVERED_RECORD) message='PPC dispatch error: uncovered handler for dispatch<1>()' ;;
    UNCOVERED_REAL_RECORD) message='PPC dispatch error: uncovered handler for dispatch<1>()' ;;
    INCOMPLETE) message='PPC completeness error: missing handler for dispatch table slot' ;;
  esac
  for variant in x86 O0 O2; do
    "$host_cc" "$prefix.$variant.o" -o "$prefix.$variant.exe"
    if "$prefix.$variant.exe" >"$prefix.$variant.stdout" 2>"$prefix.$variant.stderr"; then
      fail "$feature runtime error was not detected ($variant)"
    else
      status=$?
    fi
    [ "$status" -eq 134 ] || fail "$feature failed without SIGABRT ($variant, $status)"
    grep -Fq "$message" "$prefix.$variant.stderr" || fail "missing $feature diagnostic ($variant)"
  done
done

# Unsupported source constructs must produce a compiler diagnostic instead
# of invalid IR or a silently different executable.
for feature in VA_ARG_ATOMIC_RECORD VA_ARG_VOLATILE_RECORD UNPROTOTYPED_AGGREGATE TLS ATOMIC MIXED_X87_BITFIELD VOLATILE VOLATILE_POINTER VOLATILE_TYPEDEF VOLATILE_VLA_PARAMETER VOLATILE_VLA_ELEMENT ASM COMPUTED_GOTO; do
  echo "testing LLVM diagnostic $feature"
  if "$compiler" -S -emit-llvm -DREJECT_$feature \
      "$source_dir/test/llvm/reject.c" -o "$result_dir/rejected.ll" \
      >"$result_dir/reject-$feature.stdout" 2>"$result_dir/reject-$feature.stderr"; then
    fail "$feature was accepted by the initial LLVM backend"
  fi
  grep -q 'LLVM backend:' "$result_dir/reject-$feature.stderr" ||
    fail "$feature failed without an LLVM backend diagnostic"
done

# Driver behavior is checked separately from IR semantics.
driver_dir=$result_dir/driver
mkdir -p "$driver_dir"
cp "$source_dir/test/llvm/valid/functions.c" "$driver_dir/input.c"
(
  cd "$driver_dir"
  "$compiler" -S -emit-llvm input.c
)
[ -s "$driver_dir/input.ll" ] || fail "default output extension is not .ll"
"$llvm_as" "$driver_dir/input.ll" -o "$driver_dir/input.bc"
"$compiler" -S -emit-llvm "$driver_dir/input.c" -o - >"$driver_dir/stdout.ll"
"$llvm_as" "$driver_dir/stdout.ll" -o "$driver_dir/stdout.bc"
cp "$driver_dir/input.c" "$driver_dir/input.custom"
"$compiler" -S -emit-llvm -xc "$driver_dir/input.custom" -o "$driver_dir/explicit.ll"
"$llvm_as" "$driver_dir/explicit.ll" -o "$driver_dir/explicit.bc"

expect_driver_error() {
  label=$1
  shift
  if "$compiler" "$@" >"$driver_dir/error.stdout" 2>"$driver_dir/error.stderr"; then
    fail "driver accepted $label"
  fi
  [ -s "$driver_dir/error.stderr" ] || fail "no diagnostic for $label"
}
expect_driver_error missing-S -emit-llvm "$driver_dir/input.c"
expect_driver_error object-mode -S -c -emit-llvm "$driver_dir/input.c"
expect_driver_error preprocessing -S -E -emit-llvm "$driver_dir/input.c"
expect_driver_error dependencies -S -M -emit-llvm "$driver_dir/input.c"
expect_driver_error multiple-inputs -S -emit-llvm "$driver_dir/input.c" \
  "$source_dir/test/llvm/valid/integers.c" -o "$driver_dir/multiple.ll"
expect_driver_error object-input -S -emit-llvm "$result_dir/functions.x86.o"
"$compiler" -S "$driver_dir/input.c" -o "$driver_dir/input.s"
expect_driver_error assembler-input -S -emit-llvm "$driver_dir/input.s"

printf 'preserve existing output\n' >"$driver_dir/preserved.ll"
cp "$driver_dir/preserved.ll" "$driver_dir/preserved.expected"
expect_driver_error unsupported-output -S -emit-llvm -DREJECT_ATOMIC \
  "$source_dir/test/llvm/reject.c" -o "$driver_dir/preserved.ll"
cmp "$driver_dir/preserved.expected" "$driver_dir/preserved.ll" ||
  fail "an emission failure overwrote an existing output file"

# This real program previously needed one native switch-containing unit.
# Compile every C unit through LLVM, including with the self-hosted compiler.
classic=$source_dir/examples/evolution/figures/direct-classic/01-start
mkdir -p "$result_dir/classic-figures"
for level in 0 2; do
  prefix=$result_dir/classic-figures/O$level
  PPCHIBICC_LLVM_OPT="$llvm_opt" PPCHIBICC_LLVM_LLC="$llvm_llc" \
    "$compiler" --backend=llvm "-O$level" "$classic"/*.c -o "$prefix.exe"
  "$prefix.exe" "$classic/data/input1.txt" "$prefix.output" >"$prefix.stdout" 2>"$prefix.stderr"
  cmp "$classic/data/output1.txt" "$prefix.output" || fail 'classic figures output differs'
  [ ! -s "$prefix.stderr" ] || fail 'classic figures produced stderr'
done

# Reuse the native suite's source lists and golden outputs for all eight
# evolutionary steps, including no-ordinary-argument PPC handlers in 03/08.
for level in 0 2; do
  PPCHIBICC_LLVM_OPT="$llvm_opt" PPCHIBICC_LLVM_LLC="$llvm_llc" \
    "$source_dir/cmake/run-example-tests.sh" "$source_dir" "$compiler" \
    --backend=llvm "-O$level"
done

"$source_dir/cmake/run-llvm-driver-tests.sh" "$source_dir" "$compiler" "$host_cc" \
  "$llvm_as" "$llvm_opt" "$llvm_llc" "$result_dir/driver-pipeline"
"$source_dir/cmake/run-llvm-closed-dispatch-tests.sh" "$source_dir" "$compiler" "$host_cc" \
  "$llvm_as" "$llvm_opt" "$llvm_llc" "$result_dir/closed-dispatch"
"$source_dir/cmake/run-llvm-closed-multimethod-tests.sh" "$source_dir" "$compiler" "$host_cc" \
  "$llvm_as" "$llvm_opt" "$llvm_llc" "$result_dir/closed-multimethod"
"$source_dir/cmake/run-ppc-table-size-tests.sh" "$source_dir" "$compiler" \
  "$result_dir/table-sizes" "$llvm_opt" "$llvm_llc"
echo "LLVM tests passed"
