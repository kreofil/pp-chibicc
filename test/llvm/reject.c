#if defined(REJECT_FLOAT)
int main(void) { double x = 1.5; return (int)x; }
#elif defined(REJECT_AGGREGATE_FLOAT)
struct S { double x; };
struct S make(struct S s) { return s; }
int main(void) { struct S s = {3}; return (int)make(s).x; }
#elif defined(REJECT_VARARG_AGGREGATE)
struct S { int x; };
int take(int n, ...);
int main(void) { struct S s = {3}; return take(1, s); }
#elif defined(REJECT_BITFIELD)
int main(void) { struct S { unsigned int x : 3; } s = {3}; return s.x; }
#elif defined(REJECT_VARIADIC)
int pick(int x, ...) { return x; }
int main(void) { return pick(1, 2); }
#elif defined(REJECT_VARARG_FLOAT)
int printf(const char *format, ...);
int main(void) { return printf("%.1f", 1.0); }
#elif defined(REJECT_NO_PROTOTYPE)
int external();
int main(void) { return external(3); }
#elif defined(REJECT_VLA)
int main(void) { int n = 3; int a[n]; a[0] = 1; return a[0]; }
#elif defined(REJECT_TLS)
_Thread_local int x;
int main(void) { return x; }
#elif defined(REJECT_ATOMIC)
_Atomic int x;
int main(void) { return x; }
#elif defined(REJECT_VOLATILE)
volatile int value;
int main(void) { int a = value; int b = value; return a + b; }
#elif defined(REJECT_VOLATILE_POINTER)
int main(void) { int x = 1; int *volatile p = &x; return *p; }
#elif defined(REJECT_VOLATILE_TYPEDEF)
typedef int Values[2];
int first(volatile Values values) { return values[0]; }
int main(void) { int values[2] = {1, 2}; return first(values); }
#elif defined(REJECT_ASM)
int main(void) { asm("nop"); return 0; }
#elif defined(REJECT_COMPUTED_GOTO)
int main(void) { void *p = &&done; goto *p; done: return 0; }
#else
#error Select a REJECT_ feature
#endif
