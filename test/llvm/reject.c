#if defined(REJECT_VARARG_AGGREGATE)
struct S { int x; };
int take(int n, ...);
int main(void) { struct S s = {3}; return take(1, s); }
#elif defined(REJECT_VA_ARG_AGGREGATE)
#include <stdarg.h>
struct S { int n; };
int pick(int x, ...) { va_list ap; va_start(ap, x); return va_arg(ap, struct S).n; }
int main(void) { return 0; }
#elif defined(REJECT_UNPROTOTYPED_AGGREGATE)
struct S { int n; };
int external();
int main(void) { struct S s = {3}; return external(s); }
#elif defined(REJECT_TLS)
_Thread_local int x;
int main(void) { return x; }
#elif defined(REJECT_ATOMIC)
_Atomic int x;
int main(void) { return x; }
#elif defined(REJECT_MIXED_X87_BITFIELD)
union X { long double x; struct { unsigned long :64; unsigned b:1; } bits; };
union X identity(union X x) { return x; }
#elif defined(REJECT_VOLATILE)
volatile int value;
int main(void) { int a = value; int b = value; return a + b; }
#elif defined(REJECT_VOLATILE_POINTER)
int main(void) { int x = 1; int *volatile p = &x; return *p; }
#elif defined(REJECT_VOLATILE_TYPEDEF)
typedef int Values[2];
int first(volatile Values values) { return values[0]; }
int main(void) { int values[2] = {1, 2}; return first(values); }
#elif defined(REJECT_VOLATILE_VLA_PARAMETER)
int first(int n, int values[volatile n]) { return values[0]; }
int main(void) { int values[2] = {1, 2}; return first(2, values); }
#elif defined(REJECT_VOLATILE_VLA_ELEMENT)
int first(int n, volatile int values[n]) { return values[0]; }
int main(void) { int values[2] = {1, 2}; return first(2, values); }
#elif defined(REJECT_ASM)
int main(void) { asm("nop"); return 0; }
#elif defined(REJECT_COMPUTED_GOTO)
int main(void) { void *p = &&done; goto *p; done: return 0; }
#else
#error Select a REJECT_ feature
#endif
