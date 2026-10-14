#!/bin/bash
chibicc=$1

tmp=`mktemp -d /tmp/chibicc-test-XXXXXX`
trap 'rm -rf $tmp' INT TERM HUP EXIT
echo > $tmp/empty.c

check() {
    if [ $? -eq 0 ]; then
        echo "testing $1 ... passed"
    else
        echo "testing $1 ... failed"
        exit 1
    fi
}

expect_fail() {
    name=$1
    cat > $tmp/fail.c
    $chibicc -Iinclude -Itest -c -o $tmp/fail.o $tmp/fail.c >/dev/null 2>$tmp/fail.err
    [ $? -ne 0 ]
    check "$name"
}

expect_run_fail() {
    name=$1
    cat > $tmp/fail.c
    $chibicc -Iinclude -Itest -o $tmp/fail $tmp/fail.c >/dev/null 2>$tmp/fail.err
    [ $? -eq 0 ] || false
    check "$name build"
    sh -c "$tmp/fail" >/dev/null 2>>$tmp/fail.err
    [ $? -ne 0 ]
    check "$name"
}

expect_ppc_assign_fail() {
    name=$1
    stmt=$2
    cat > $tmp/fail.c <<EOF
typedef struct Rectangle { int x, y; } Rectangle;
typedef struct Triangle { int a, b, c; } Triangle;
typedef struct Figure {} case {} Figure;
Figure + { Triangle t; };
Figure + { Rectangle r; };
int main() {
  Figure f;
  Figure.t t;
  Figure.r r;
  $stmt
  return 0;
}
EOF
    $chibicc -Iinclude -Itest -c -o $tmp/fail.o $tmp/fail.c >/dev/null 2>$tmp/fail.err
    [ $? -ne 0 ]
    check "$name"
}

# -o
rm -f $tmp/out
./chibicc -c -o $tmp/out $tmp/empty.c
[ -f $tmp/out ]
check -o

# --help
$chibicc --help 2>&1 | grep -q chibicc
check --help

# -S
echo 'int main() {}' | $chibicc -S -o- -xc - | grep -q 'main:'
check -S

# Default output file
rm -f $tmp/out.o $tmp/out.s
echo 'int main() {}' > $tmp/out.c
(cd $tmp; $OLDPWD/$chibicc -c out.c)
[ -f $tmp/out.o ]
check 'default output file'

(cd $tmp; $OLDPWD/$chibicc -c -S out.c)
[ -f $tmp/out.s ]
check 'default output file'

# Multiple input files
rm -f $tmp/foo.o $tmp/bar.o
echo 'int x;' > $tmp/foo.c
echo 'int y;' > $tmp/bar.c
(cd $tmp; $OLDPWD/$chibicc -c $tmp/foo.c $tmp/bar.c)
[ -f $tmp/foo.o ] && [ -f $tmp/bar.o ]
check 'multiple input files'

rm -f $tmp/foo.s $tmp/bar.s
echo 'int x;' > $tmp/foo.c
echo 'int y;' > $tmp/bar.c
(cd $tmp; $OLDPWD/$chibicc -c -S $tmp/foo.c $tmp/bar.c)
[ -f $tmp/foo.s ] && [ -f $tmp/bar.s ]
check 'multiple input files'

# Run linker
rm -f $tmp/foo
echo 'int main() { return 0; }' | $chibicc -o $tmp/foo -xc -xc -
$tmp/foo
check linker

rm -f $tmp/foo
echo 'int bar(); int main() { return bar(); }' > $tmp/foo.c
echo 'int bar() { return 42; }' > $tmp/bar.c
$chibicc -o $tmp/foo $tmp/foo.c $tmp/bar.c
$tmp/foo
[ "$?" = 42 ]
check linker

# a.out
rm -f $tmp/a.out
echo 'int main() {}' > $tmp/foo.c
(cd $tmp; $OLDPWD/$chibicc foo.c)
[ -f $tmp/a.out ]
check a.out

# -E
echo foo > $tmp/out
echo "#include \"$tmp/out\"" | $chibicc -E -xc - | grep -q foo
check -E

echo foo > $tmp/out1
echo "#include \"$tmp/out1\"" | $chibicc -E -o $tmp/out2 -xc -
cat $tmp/out2 | grep -q foo
check '-E and -o'

# -I
mkdir $tmp/dir
echo foo > $tmp/dir/i-option-test
echo "#include \"i-option-test\"" | $chibicc -I$tmp/dir -E -xc - | grep -q foo
check -I

# -D
echo foo | $chibicc -Dfoo -E -xc - | grep -q 1
check -D

# -D
echo foo | $chibicc -Dfoo=bar -E -xc - | grep -q bar
check -D

# -U
echo foo | $chibicc -Dfoo=bar -Ufoo -E -xc - | grep -q foo
check -U

# ignored options
$chibicc -c -O -Wall -g -std=c11 -ffreestanding -fno-builtin \
         -fno-omit-frame-pointer -fno-stack-protector -fno-strict-aliasing \
         -m64 -mno-red-zone -w -o /dev/null $tmp/empty.c
check 'ignored options'

# BOM marker
printf '\xef\xbb\xbfxyz\n' | $chibicc -E -o- -xc - | grep -q '^xyz'
check 'BOM marker'

# Inline functions
echo 'inline void foo() {}' > $tmp/inline1.c
echo 'inline void foo() {}' > $tmp/inline2.c
echo 'int main() { return 0; }' > $tmp/inline3.c
$chibicc -o /dev/null $tmp/inline1.c $tmp/inline2.c $tmp/inline3.c
check inline

echo 'extern inline void foo() {}' > $tmp/inline1.c
echo 'int foo(); int main() { foo(); }' > $tmp/inline2.c
$chibicc -o /dev/null $tmp/inline1.c $tmp/inline2.c
check inline

echo 'static inline void f1() {}' | $chibicc -o- -S -xc - | grep -v -q f1:
check inline

echo 'static inline void f1() {} void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q f1:
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q f1:
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -v -q f2:
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q f1:
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q f2:
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() {}' | $chibicc -o- -S -xc - | grep -v -q f1:
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() {}' | $chibicc -o- -S -xc - | grep -v -q f2:
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q f1:
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q f2:
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q f1:
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q f2:
check inline

# -idirafter
mkdir -p $tmp/dir1 $tmp/dir2
echo foo > $tmp/dir1/idirafter
echo bar > $tmp/dir2/idirafter
echo "#include \"idirafter\"" | $chibicc -I$tmp/dir1 -I$tmp/dir2 -E -xc - | grep -q foo
check -idirafter
echo "#include \"idirafter\"" | $chibicc -idirafter $tmp/dir1 -I$tmp/dir2 -E -xc - | grep -q bar
check -idirafter

# -fcommon
echo 'int foo;' | $chibicc -S -o- -xc - | grep -q '\.comm foo'
check '-fcommon (default)'

echo 'int foo;' | $chibicc -fcommon -S -o- -xc - | grep -q '\.comm foo'
check '-fcommon'

# -fno-common
echo 'int foo;' | $chibicc -fno-common -S -o- -xc - | grep -q '^foo:'
check '-fno-common'

# -include
echo foo > $tmp/out.h
echo bar | $chibicc -include $tmp/out.h -E -o- -xc - | grep -q -z 'foo.*bar'
check -include
echo NULL | $chibicc -Iinclude -include stdio.h -E -o- -xc - | grep -q 0
check -include

# -x
echo 'int x;' | $chibicc -c -xc -o $tmp/foo.o -
check -xc
echo 'x:' | $chibicc -c -x assembler -o $tmp/foo.o -
check '-x assembler'

echo 'int x;' > $tmp/foo.c
$chibicc -c -x assembler -x none -o $tmp/foo.o $tmp/foo.c
check '-x none'

expect_ppc_assign_fail 'PPC rejects specialization assignment t=r' 't = r;'
expect_ppc_assign_fail 'PPC rejects specialization assignment r=t' 'r = t;'
expect_ppc_assign_fail 'PPC rejects generalization assignment f=r' 'f = r;'
expect_ppc_assign_fail 'PPC rejects generalization assignment f=t' 'f = t;'
expect_ppc_assign_fail 'PPC rejects specialization assignment t=f' 't = f;'
expect_ppc_assign_fail 'PPC rejects specialization assignment r=f' 'r = f;'
expect_ppc_assign_fail 'PPC rejects specialization initializer' 'Figure.t u = r;'
expect_fail 'PPC case void generalization assignment remains strict' <<'EOF'
struct CaseSwitch {} case { void on; };
int main() {
  struct CaseSwitch s;
  struct CaseSwitch.on on;
  s = on;
  return 0;
}
EOF
expect_fail 'PPC enum rejects specialization assignment from zero' <<'EOF'
struct EnumSwitch {} enum { on, off };
int main() {
  struct EnumSwitch s;
  struct EnumSwitch.on on;
  on = s;
  return 0;
}
EOF
expect_fail 'PPC enum rejects different specialization assignment' <<'EOF'
struct EnumSwitch {} enum { on, off };
int main() {
  struct EnumSwitch.on on;
  struct EnumSwitch.off off;
  off = on;
  return 0;
}
EOF
expect_fail 'PPC enum rejects typed specialization extension' <<'EOF'
struct Payload { int x; };
struct EnumSwitch {} enum { on };
struct EnumSwitch + { struct Payload payload; };
int main() { return 0; }
EOF
expect_fail 'PPC rejects enum feature extension after payload specialization' <<'EOF'
struct Payload { int x; };
struct MixedSwitch {} case { struct Payload payload; };
struct MixedSwitch + { extra };
int main() { return 0; }
EOF
expect_fail 'PPC rejects typedef const generalization extension' <<'EOF'
typedef struct ClosedFigure {} case { int first; } const ClosedFigure;
ClosedFigure + { int second; };
int main() { return 0; }
EOF
expect_fail 'PPC rejects enum const generalization extension' <<'EOF'
typedef struct ClosedEnum {} enum { first } const ClosedEnum;
ClosedEnum + { second };
int main() { return 0; }
EOF
expect_fail 'PPC rejects tagged const generalization extension' <<'EOF'
struct ClosedTag {} case { void first; } const;
struct ClosedTag + { void second; };
int main() { return 0; }
EOF
expect_fail 'PPC rejects obsolete disabled zero syntax' <<'EOF'
typedef struct ClosedEnum { int n; } case { void one; } = 0 ClosedEnum;
int main() { return 0; }
EOF
expect_fail 'PPC rejects .void when default specialization is named' <<'EOF'
typedef struct DefaultFigure {} case {
  default struct { int x, y; } rect;
  int number;
} DefaultFigure;
int main() {
  DefaultFigure.void e;
  return 0;
}
EOF
expect_fail 'PPC rejects enum feature extension after default specialization' <<'EOF'
typedef struct DefaultFigure {} case {
  default void zero;
  void one;
} DefaultFigure;
DefaultFigure + { two };
int main() { return 0; }
EOF
expect_fail 'PPC rejects default specialization after ordinary case item' <<'EOF'
typedef struct DefaultFigure {} case {
  int number;
  default int zero;
} DefaultFigure;
int main() { return 0; }
EOF
expect_fail 'PPC default generalization assignment remains strict for nonzero specialization' <<'EOF'
typedef struct DefaultFigure {} case {
  default struct { int x, y; } rect;
  int number;
} DefaultFigure;
int main() {
  DefaultFigure f;
  DefaultFigure.number n;
  f = n;
  return 0;
}
EOF
expect_run_fail 'PPC rejects get_spec_ptr ordinary lower bound' <<'EOF'
typedef struct OpenEnum { int n; } case { void one; } OpenEnum;
int main() {
  OpenEnum *e = get_spec_ptr(OpenEnum, -1);
  return *(int *)e;
}
EOF
expect_run_fail 'PPC rejects get_spec_ptr ordinary upper bound' <<'EOF'
typedef struct OpenEnum { int n; } case { void one; } OpenEnum;
int main() {
  OpenEnum *e = get_spec_ptr(OpenEnum, 2);
  return *(int *)e;
}
EOF
expect_run_fail 'PPC rejects default get_spec_ptr upper bound' <<'EOF'
typedef struct DefaultEnum { int n; } case {
  default void zero;
  void one;
} DefaultEnum;
int main() {
  DefaultEnum *e = get_spec_ptr(DefaultEnum, 2);
  return *(int *)e;
}
EOF
expect_run_fail 'PPC rejects incomplete const handler table' <<'EOF'
typedef struct ClosedSwitch {} case { void on; void off; } const ClosedSwitch;
int ClosedSwitchCode<ClosedSwitch *s>() = 0;
int ClosedSwitchCode<ClosedSwitch.void *s>() { return 0; }
int ClosedSwitchCode<ClosedSwitch.on *s>() { return 1; }
int main() {
  return 0;
}
EOF
expect_run_fail 'PPC rejects incomplete const multimethod table' <<'EOF'
typedef struct ClosedBit {} case { void one; } const ClosedBit;
int ClosedBitPair<ClosedBit *a, ClosedBit *b>() = 0;
int ClosedBitPair<ClosedBit.void *a, ClosedBit.void *b>() { return 0; }
int ClosedBitPair<ClosedBit.void *a, ClosedBit.one *b>() { return 1; }
int ClosedBitPair<ClosedBit.one *a, ClosedBit.void *b>() { return 2; }
int main() { return 0; }
EOF
expect_fail 'PPC rejects specialization argument by value' <<'EOF'
typedef struct Rectangle { int x, y; } Rectangle;
typedef struct Triangle { int a, b, c; } Triangle;
typedef struct Figure {} case {} Figure;
Figure + { Triangle t; };
Figure + { Rectangle r; };
void take(Figure.t t) {}
int main() {
  Figure.r r;
  take(r);
  return 0;
}
EOF
expect_fail 'PPC rejects specialization return by value' <<'EOF'
typedef struct Rectangle { int x, y; } Rectangle;
typedef struct Triangle { int a, b, c; } Triangle;
typedef struct Figure {} case {} Figure;
Figure + { Triangle t; };
Figure + { Rectangle r; };
Figure.t make(void) {
  Figure.r r;
  return r;
}
int main() { return 0; }
EOF
expect_fail 'PPC rejects specialization conditional value' <<'EOF'
typedef struct Rectangle { int x, y; } Rectangle;
typedef struct Triangle { int a, b, c; } Triangle;
typedef struct Figure {} case {} Figure;
Figure + { Triangle t; };
Figure + { Rectangle r; };
int main() {
  Figure.t t;
  Figure.r r;
  (1 ? t : r);
  return 0;
}
EOF

# -E
echo foo | $chibicc -E - | grep -q foo
check -E

# .a file
echo 'void foo() {}' | $chibicc -c -xc -o $tmp/foo.o -
echo 'void bar() {}' | $chibicc -c -xc -o $tmp/bar.o -
ar rcs $tmp/foo.a $tmp/foo.o $tmp/bar.o
echo 'void foo(); void bar(); int main() { foo(); bar(); }' > $tmp/main.c
$chibicc -o $tmp/foo $tmp/main.c $tmp/foo.a
check '.a'

# .so file
echo 'void foo() {}' | cc -fPIC -c -xc -o $tmp/foo.o -
echo 'void bar() {}' | cc -fPIC -c -xc -o $tmp/bar.o -
cc -shared -o $tmp/foo.so $tmp/foo.o $tmp/bar.o
echo 'void foo(); void bar(); int main() { foo(); bar(); }' > $tmp/main.c
$chibicc -o $tmp/foo $tmp/main.c $tmp/foo.so
check '.so'

$chibicc -hashmap-test
check 'hashmap'

# -M
echo '#include "out2.h"' > $tmp/out.c
echo '#include "out3.h"' >> $tmp/out.c
touch $tmp/out2.h $tmp/out3.h
$chibicc -M -I$tmp $tmp/out.c | grep -q -z '^out.o: .*/out\.c .*/out2\.h .*/out3\.h'
check -M

# -MF
$chibicc -MF $tmp/mf -M -I$tmp $tmp/out.c
grep -q -z '^out.o: .*/out\.c .*/out2\.h .*/out3\.h' $tmp/mf
check -MF

# -MP
$chibicc -MF $tmp/mp -MP -M -I$tmp $tmp/out.c
grep -q '^.*/out2.h:' $tmp/mp
check -MP
grep -q '^.*/out3.h:' $tmp/mp
check -MP

# -MT
$chibicc -MT foo -M -I$tmp $tmp/out.c | grep -q '^foo:'
check -MT
$chibicc -MT foo -MT bar -M -I$tmp $tmp/out.c | grep -q '^foo bar:'
check -MT

# -MD
echo '#include "out2.h"' > $tmp/md2.c
echo '#include "out3.h"' > $tmp/md3.c
(cd $tmp; $OLDPWD/$chibicc -c -MD -I. md2.c md3.c)
grep -q -z '^md2.o:.* md2\.c .* ./out2\.h' $tmp/md2.d
check -MD
grep -q -z '^md3.o:.* md3\.c .* ./out3\.h' $tmp/md3.d
check -MD

$chibicc -c -MD -MF $tmp/md-mf.d -I. $tmp/md2.c
grep -q -z '^md2.o:.*md2\.c .*/out2\.h' $tmp/md-mf.d
check -MD

echo 'extern int bar; int foo() { return bar; }' | $chibicc -fPIC -xc -c -o $tmp/foo.o -
cc -shared -o $tmp/foo.so $tmp/foo.o
echo 'int foo(); int bar=3; int main() { foo(); }' > $tmp/main.c
$chibicc -o $tmp/foo $tmp/main.c $tmp/foo.so
check -fPIC

# #include_next
mkdir -p $tmp/next1 $tmp/next2 $tmp/next3
echo '#include "file1.h"' > $tmp/file.c
echo '#include_next "file1.h"' > $tmp/next1/file1.h
echo '#include_next "file2.h"' > $tmp/next2/file1.h
echo 'foo' > $tmp/next3/file2.h
$chibicc -I$tmp/next1 -I$tmp/next2 -I$tmp/next3 -E $tmp/file.c | grep -q foo
check '#include_next'

# -static
echo 'extern int bar; int foo() { return bar; }' > $tmp/foo.c
echo 'int foo(); int bar=3; int main() { foo(); }' > $tmp/bar.c
$chibicc -static -o $tmp/foo $tmp/foo.c $tmp/bar.c
check -static
file $tmp/foo | grep -q 'statically linked'
check -static

# -shared
echo 'extern int bar; int foo() { return bar; }' > $tmp/foo.c
echo 'int foo(); int bar=3; int main() { foo(); }' > $tmp/bar.c
$chibicc -fPIC -shared -o $tmp/foo.so $tmp/foo.c $tmp/bar.c
check -shared

# -L
echo 'extern int bar; int foo() { return bar; }' > $tmp/foo.c
$chibicc -fPIC -shared -o $tmp/libfoobar.so $tmp/foo.c
echo 'int foo(); int bar=3; int main() { foo(); }' > $tmp/bar.c
$chibicc -o $tmp/foo $tmp/bar.c -L$tmp -lfoobar
check -L

# -Wl,
echo 'int foo() {}' | $chibicc -c -o $tmp/foo.o -xc -
echo 'int foo() {}' | $chibicc -c -o $tmp/bar.o -xc -
echo 'int main() {}' | $chibicc -c -o $tmp/baz.o -xc -
cc -Wl,-z,muldefs,--gc-sections -o $tmp/foo $tmp/foo.o $tmp/bar.o $tmp/baz.o
check -Wl,

# -Xlinker
echo 'int foo() {}' | $chibicc -c -o $tmp/foo.o -xc -
echo 'int foo() {}' | $chibicc -c -o $tmp/bar.o -xc -
echo 'int main() {}' | $chibicc -c -o $tmp/baz.o -xc -
cc -Xlinker -z -Xlinker muldefs -Xlinker --gc-sections -o $tmp/foo $tmp/foo.o $tmp/bar.o $tmp/baz.o
check -Xlinker

echo OK
