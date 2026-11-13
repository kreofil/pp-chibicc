#!/bin/sh
set -eu
source_dir=$(realpath "$1")
compiler=$(realpath "$2")
mkdir -p "$3"
result_dir=$(realpath "$3")
variants=native
if [ "$#" -ge 5 ]; then
  export PPCHIBICC_LLVM_OPT="$4" PPCHIBICC_LLVM_LLC="$5"
  variants='native O0 O2'
fi
fail() { echo "VLA parameter tests: $*" >&2; exit 1; }

reject() {
  name=$1
  message=$2
  cat >"$result_dir/$name.c"
  for backend in native llvm; do
    set -- -c
    [ "$backend" = native ] || set -- -S -emit-llvm
    status=0
    "$compiler" "$@" "$result_dir/$name.c" -o "$result_dir/$name.$backend.out" \
      >"$result_dir/$name.$backend.stdout" 2>"$result_dir/$name.$backend.stderr" || status=$?
    [ "$status" -eq 1 ] || fail "$name ($backend): expected diagnostic exit 1, got $status"
    grep -Fq "$message" "$result_dir/$name.$backend.stderr" || fail "$name ($backend): missing diagnostic"
    [ ! -s "$result_dir/$name.$backend.stdout" ] || fail "$name ($backend): unexpected stdout"
  done
}

reject duplicate 'duplicate parameter name' <<'EOF'
int f(int n, int n);
EOF
reject duplicate-polymorphic 'duplicate parameter name' <<'EOF'
typedef struct T {} enum { one } const T;
int f<T *x>(int x);
EOF
reject leaked-prototype 'undefined variable' <<'EOF'
int f(int private, int a[private]);
int values[private];
EOF
reject forward-name 'undefined variable' <<'EOF'
int f(int a[later], int later);
EOF
reject leaked-callback 'undefined variable' <<'EOF'
int f(int (*callback)(int private), int a[private]);
EOF
reject star-local '[*] requires function prototype scope' <<'EOF'
int main(void) { int a[*]; return 0; }
EOF
reject star-definition '[*] is not allowed in a function definition' <<'EOF'
int f(int n, int a[*]) { return 0; }
EOF
reject star-row-definition '[*] is not allowed in a function definition' <<'EOF'
int f(int n, int a[n][*]) { return 0; }
EOF
reject static-star 'static array parameter requires a bound' <<'EOF'
int f(int a[static *]);
EOF
reject static-empty 'static array parameter requires a bound' <<'EOF'
int f(int a[static]);
EOF
reject local-qualifier 'array qualifiers require a function parameter' <<'EOF'
int main(void) { int n = 3; int a[restrict n]; return 0; }
EOF
reject inner-qualifier 'array qualifiers require the outermost parameter array' <<'EOF'
int f(int n, int a[2][restrict n]);
EOF
reject pointer-qualifier 'array qualifiers require the outermost parameter array' <<'EOF'
int f(int n, int (*a)[static n]);
EOF
reject incompatible-row 'different ordinary parameters' <<'EOF'
typedef struct T {} enum { one } const T;
int f<T *x>(int n, int a[n][3]);
int f<T.one *x>(int n, int a[n][4]);
EOF
reject incompatible-element 'different ordinary parameters' <<'EOF'
typedef struct T {} enum { one } const T;
int f<T *x>(int n, int a[n][n]);
int f<T.one *x>(int n, double a[n][n]);
EOF

for mode in runtime table switch; do
  for variant in $variants; do
    set --
    case "$variant" in
      O0|O2) set -- --backend=llvm "-$variant" ;;
    esac
    exe=$result_dir/$mode.$variant.exe
    "$compiler" "$@" "--ppc-dispatch=$mode" "$source_dir/test/ppc-vla-params.c" -o "$exe"
    "$exe" >"$exe.stdout" 2>"$exe.stderr" || fail "$mode $variant execution"
    cmp "$source_dir/test/llvm/valid/ppc-vla-params.expected" "$exe.stdout" || fail "$mode $variant output"
    [ ! -s "$exe.stderr" ] || fail "$mode $variant stderr"
  done
done
echo 'VLA parameter checks passed'
