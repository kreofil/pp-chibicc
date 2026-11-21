#if defined(REJECT_VA_ARG_ATOMIC_RECORD)
#include <stdarg.h>
struct S { _Atomic int n; };
int pick(int x, ...) { va_list ap; va_start(ap,x); return va_arg(ap,struct S).n; }
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
#elif defined(REJECT_ASM)
int main(void) { asm("nop"); return 0; }
#elif defined(REJECT_COMPUTED_GOTO)
int main(void) { void *p = &&done; goto *p; done: return 0; }
#else
#error Select a REJECT_ feature
#endif
