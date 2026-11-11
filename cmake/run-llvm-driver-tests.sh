#!/bin/sh
set -eu
export LC_ALL=C

source_dir=$(realpath "$1")
compiler=$(realpath "$2")
host_cc=$3
llvm_as=$4
llvm_opt=$5
llvm_llc=$6
mkdir -p "$7"
result_dir=$(realpath "$7")
export PPCHIBICC_LLVM_OPT="$llvm_opt" PPCHIBICC_LLVM_LLC="$llvm_llc"
fail() { echo "LLVM driver test: $*" >&2; exit 1; }
llvm_major() {
  "$1" --version | sed -n 's/.*LLVM version \([0-9][0-9]*\)\..*/\1/p' | head -n 1
}
major=$(llvm_major "$llvm_as")
[ -n "$major" ] && [ "$major" -ge 19 ] || fail 'LLVM 19 or newer is required'
[ "$(llvm_major "$llvm_opt")" = "$major" ] && [ "$(llvm_major "$llvm_llc")" = "$major" ] ||
  fail 'all LLVM test tools must have the same major version'

check_hello() {
  "$1" >"$result_dir/hello.stdout" 2>"$result_dir/hello.stderr"
  printf 'Hello from ppchibicc LLVM IR\n' >"$result_dir/hello.expected"
  cmp "$result_dir/hello.expected" "$result_dir/hello.stdout" || fail "incorrect hello output"
  [ ! -s "$result_dir/hello.stderr" ] || fail "unexpected hello stderr"
}

reject() {
  if "$compiler" "$@" >"$result_dir/reject.stdout" 2>"$result_dir/reject.stderr"; then
    fail "unexpected success: $*"
  fi
  [ -s "$result_dir/reject.stderr" ] || fail "missing diagnostic: $*"
}

echo 'testing LLVM driver: optimization levels, objects, IR and linking'
hello=$source_dir/examples/llvm/hello.c
for level in 0 1 2 3 s z; do
  "$compiler" --backend=llvm "-O$level" "$hello" -o "$result_dir/hello-O$level.exe"
  check_hello "$result_dir/hello-O$level.exe"
done
"$compiler" --backend=llvm "$hello" -o "$result_dir/hello-default.exe"
check_hello "$result_dir/hello-default.exe"
"$compiler" --backend=llvm -O "$hello" -o "$result_dir/hello-O.exe"
check_hello "$result_dir/hello-O.exe"

"$compiler" --backend=llvm -O2 -c "$hello" -o "$result_dir/hello.o"
"$compiler" "$result_dir/hello.o" -o "$result_dir/hello-object.exe"
check_hello "$result_dir/hello-object.exe"
"$host_cc" "$result_dir/hello.o" -o "$result_dir/hello-host.exe"
check_hello "$result_dir/hello-host.exe"
"$compiler" --backend=llvm -O2 -S "$hello" -o "$result_dir/hello.s"
grep -q '^main:' "$result_dir/hello.s" || fail 'missing assembly function'
"$compiler" --backend=llvm -O2 -S "$hello" -o - >"$result_dir/stdout.s"
cmp "$result_dir/hello.s" "$result_dir/stdout.s" || fail 'assembly stdout differs'
"$compiler" --backend=llvm -c "$hello" -o - >"$result_dir/stdout.o"
"$host_cc" "$result_dir/stdout.o" -o "$result_dir/stdout.exe"
check_hello "$result_dir/stdout.exe"

"$compiler" -S -emit-llvm "$hello" -o "$result_dir/hello.ll"
"$llvm_as" "$result_dir/hello.ll" -o "$result_dir/hello.bc"
for kind in ll bc; do
  "$compiler" --backend=llvm -O2 "$result_dir/hello.$kind" -o "$result_dir/hello-$kind.exe"
  check_hello "$result_dir/hello-$kind.exe"
  "$compiler" --backend=llvm -Oz "$result_dir/hello.$kind" -o "$result_dir/hello-$kind-size.exe"
  check_hello "$result_dir/hello-$kind-size.exe"
done
"$compiler" --backend=llvm -O2 -x ir - -o "$result_dir/stdin-ir.exe" <"$result_dir/hello.ll"
check_hello "$result_dir/stdin-ir.exe"
"$compiler" --backend=llvm -O2 -x c - -o "$result_dir/stdin-c.exe" <"$hello"
check_hello "$result_dir/stdin-c.exe"
reject "$result_dir/hello.ll" -o "$result_dir/rejected.exe"
reject --backend=missing "$hello"
reject --backend=x86-64 -S -emit-llvm "$hello"
for flag in -O4 -Ofast -Ogarbage; do reject --backend=llvm "$flag" "$hello"; done
printf 'invalid LLVM IR\n' >"$result_dir/invalid.ll"
reject --backend=llvm -c "$result_dir/invalid.ll" -o "$result_dir/invalid.o"
[ ! -e "$result_dir/invalid.o" ] || fail 'invalid IR left an object'

mkdir -p "$result_dir/default-names"
(
  cd "$result_dir/default-names"
  "$compiler" --backend=llvm -c "$hello"
  [ -s hello.o ] || fail 'missing default object name'
  "$compiler" --backend=llvm -S "$hello"
  [ -s hello.s ] || fail 'missing default assembly name'
  "$compiler" --backend=llvm "$hello"
  check_hello "$result_dir/default-names/a.out"
  rm a.out
  "$compiler" --backend=llvm -c hello.o
  [ ! -e a.out ] || fail '-c linked an existing object'
  "$compiler" --backend=llvm -S hello.o
  [ ! -e a.out ] || fail '-S linked an existing object'
)

echo 'testing LLVM driver: separate PPC units, mixed objects and archives'
multi=$source_dir/test/llvm/multifile

# Source suffixes select PPC automatically; headers remain includes. Preserve
# the real multifile program, replacing only its filenames/include spellings.
extensions=$result_dir/extensions
mkdir -p "$extensions"
for unit in base rectangle square; do
  sed 's/"base.h"/"base.pph"/;s/"rectangle.h"/"rectangle.pph"/;s/"square.h"/"square.pph"/' \
    "$multi/$unit.h" >"$extensions/$unit.pph"
done
for unit in base rectangle square main; do
  sed 's/"base.h"/"base.pph"/;s/"rectangle.h"/"rectangle.pph"/;s/"square.h"/"square.pph"/' \
    "$multi/$unit.c" >"$extensions/$unit.ppc"
done
for level in 0 2; do
  "$compiler" --backend=llvm "-O$level" "$extensions"/*.ppc -o "$extensions/O$level.exe"
  "$extensions/O$level.exe" >"$extensions/O$level.stdout"
  cmp "$multi/expected.txt" "$extensions/O$level.stdout" || fail '.ppc/.pph multifile output'
done
"$compiler" -S -emit-llvm "$extensions/base.ppc" -o "$extensions/base.ll"
"$llvm_as" "$extensions/base.ll" -o "$extensions/base.bc"
"$compiler" --backend=llvm -M "$extensions/main.ppc" >"$extensions/main.d"
grep -q 'base.pph' "$extensions/main.d" || fail '.pph dependency missing'
"$compiler" --backend=llvm -E "$extensions/base.pph" -o "$extensions/base.i"
printf 'preserve header output\n' >"$extensions/header.o"
cp "$extensions/header.o" "$extensions/header.expected"
reject --backend=llvm -c "$extensions/base.pph" -o "$extensions/header.o"
cmp "$extensions/header.expected" "$extensions/header.o" || fail 'header diagnostic overwrote output'
"$compiler" --backend=llvm -x c -c "$extensions/base.pph" -o "$extensions/header.o"
cp "$hello" "$extensions/hello.custom"
reject --backend=llvm -c "$extensions/hello.custom" -o "$extensions/custom.o"
"$compiler" --backend=llvm -x c "$extensions/hello.custom" -o "$extensions/custom.exe"
check_hello "$extensions/custom.exe"

"$compiler" --backend=llvm -O2 "$multi/base.c" "$multi/rectangle.c" "$multi/square.c" "$multi/main.c" \
  -o "$result_dir/multifile.exe"
"$result_dir/multifile.exe" >"$result_dir/multifile.stdout"
cmp "$multi/expected.txt" "$result_dir/multifile.stdout" || fail 'multifile output'
"$compiler" -c "$multi/rectangle.c" -o "$result_dir/rectangle.native.o"
"$compiler" --backend=llvm -O2 "$multi/base.c" "$result_dir/rectangle.native.o" "$multi/square.c" "$multi/main.c" \
  -o "$result_dir/mixed.exe"
"$result_dir/mixed.exe" >"$result_dir/mixed.stdout"
cmp "$multi/expected.txt" "$result_dir/mixed.stdout" || fail 'mixed PPC output'
for unit in base rectangle square; do
  "$compiler" --backend=llvm -O2 -c "$multi/$unit.c" -o "$result_dir/$unit.o"
done
ar rcs "$result_dir/shapes.a" "$result_dir/base.o" "$result_dir/rectangle.o" "$result_dir/square.o"
"$compiler" --backend=llvm -O2 "$multi/main.c" -Wl,--whole-archive "$result_dir/shapes.a" \
  -Wl,--no-whole-archive -o "$result_dir/archive.exe"
"$result_dir/archive.exe" >"$result_dir/archive.stdout"
cmp "$multi/expected.txt" "$result_dir/archive.stdout" || fail 'archive PPC output'

echo 'testing LLVM driver: dependency files, path spaces, shared/static and assembler inputs'
mkdir -p "$result_dir/path with spaces"
printf '#define VALUE 7\n' >"$result_dir/path with spaces/value.h"
printf '#include "value.h"\nint value(void) { return VALUE; }\n' >"$result_dir/path with spaces/value.c"
"$compiler" --backend=llvm -O2 -MMD -MF "$result_dir/space.d" -c \
  "$result_dir/path with spaces/value.c" -o "$result_dir/path with spaces/value.o"
grep -q 'value.h' "$result_dir/space.d" || fail 'missing header dependency'
if grep -q '/tmp/chibicc-' "$result_dir/space.d"; then fail 'dependency contains compiler temporary'; fi
printf 'int value(void); int main(void) { return value() != 7; }\n' >"$result_dir/value-main.c"
special_source="$result_dir/path with spaces/quote\"slash\\кириллица.c"
printf 'int value(void) { return 7; }\n' >"$special_source"
"$compiler" --backend=llvm -O2 -c "$special_source" -o "$result_dir/special-name.o"
"$host_cc" "$result_dir/value-main.c" "$result_dir/special-name.o" -o "$result_dir/special-name.exe"
"$result_dir/special-name.exe" || fail 'quoted source filename result'
"$compiler" --backend=llvm -O2 -shared "$result_dir/path with spaces/value.c" -o "$result_dir/libvalue.so"
"$compiler" --backend=llvm -O2 "$result_dir/value-main.c" -L"$result_dir" -lvalue \
  "-Wl,-rpath,$result_dir" -o "$result_dir/shared.exe"
"$result_dir/shared.exe" || fail 'shared library result'
printf 'int main(void) { return 0; }\n' >"$result_dir/zero.c"
"$compiler" --backend=llvm -O2 -static "$result_dir/zero.c" -o "$result_dir/static.exe"
"$result_dir/static.exe" || fail 'static executable result'
printf '.text\n.globl value\nvalue:\n  mov $7, %%eax\n  ret\n.section .note.GNU-stack,"",@progbits\n' >"$result_dir/value.s"
for backend in llvm x86-64; do
  "$compiler" "--backend=$backend" "$result_dir/value-main.c" "$result_dir/value.s" -o "$result_dir/asm-$backend.exe"
  "$result_dir/asm-$backend.exe" || fail 'assembler input was not linked'
done

echo 'testing LLVM driver: optional tools, argument boundaries and failed-tool cleanup'
(
  export PPCHIBICC_LLVM_OPT="$result_dir/missing-opt" PPCHIBICC_LLVM_LLC="$result_dir/missing-llc"
  "$compiler" --backend=x86-64 "$hello" -o "$result_dir/native-no-llvm.exe"
  check_hello "$result_dir/native-no-llvm.exe"
  "$compiler" --backend=llvm --backend=x86-64 "$hello" -o "$result_dir/last-backend.exe"
  check_hello "$result_dir/last-backend.exe"
  "$compiler" --backend=llvm "$result_dir/hello.o" -o "$result_dir/link-no-llvm.exe"
  check_hello "$result_dir/link-no-llvm.exe"
  "$compiler" --backend=llvm -E "$result_dir/zero.c" -o "$result_dir/zero.i"
  "$compiler" --backend=llvm -M "$result_dir/zero.c" -o "$result_dir/zero.d"
  "$compiler" --backend=llvm -O2 -S -emit-llvm "$hello" -o "$result_dir/raw-no-tools.ll"
  cmp "$result_dir/hello.ll" "$result_dir/raw-no-tools.ll" || fail 'raw IR mode changed'
  reject --backend=llvm -c "$hello" -o "$result_dir/missing.o"
  [ ! -e "$result_dir/missing.o" ] || fail 'missing tool left an object'
)

# A tool path with spaces must be one exec argument, including for stage2.
ln -sf "$llvm_opt" "$result_dir/path with spaces/opt tool"
ln -sf "$llvm_llc" "$result_dir/path with spaces/llc tool"
PPCHIBICC_LLVM_OPT="$result_dir/path with spaces/opt tool" \
  PPCHIBICC_LLVM_LLC="$result_dir/path with spaces/llc tool" \
  "$compiler" --backend=llvm -O2 "$hello" -o "$result_dir/space-tools.exe"
check_hello "$result_dir/space-tools.exe"

cat >"$result_dir/failing-llc" <<'EOF'
#!/bin/sh
for arg do last=$arg; done
printf partial >"$last"
exit 42
EOF
chmod +x "$result_dir/failing-llc"
printf sentinel >"$result_dir/preserved.o"
if PPCHIBICC_LLVM_LLC="$result_dir/failing-llc" "$compiler" --backend=llvm -O2 -### -c \
    "$hello" -o "$result_dir/preserved.o" >"$result_dir/failure.stdout" 2>"$result_dir/failure.stderr"; then
  fail 'failing llc was accepted'
fi
[ "$(cat "$result_dir/preserved.o")" = sentinel ] || fail 'failed tool overwrote output'
grep -q -- '-passes=default<O2>' "$result_dir/failure.stderr" || fail 'missing opt pipeline trace'
grep -q -- '-filetype=obj' "$result_dir/failure.stderr" || fail 'missing object trace'
if grep -q '^as ' "$result_dir/failure.stderr"; then fail 'LLVM object used the system assembler'; fi
for path in $(awk '{ for (i = 1; i <= NF; i++) if ($i ~ /^\/tmp\/chibicc-[[:alnum:]]+$/) print $i }' \
    "$result_dir/failure.stderr"); do
  [ ! -e "$path" ] || fail "temporary file leaked: $path"
done

echo 'LLVM driver tests passed'
