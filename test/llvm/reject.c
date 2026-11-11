#if defined(REJECT_LONG_DOUBLE)
int main(void) { long double x = 1.5L; return (int)x; }
#elif defined(REJECT_AGGREGATE_LONG_DOUBLE)
struct S { long double x; };
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
#elif defined(REJECT_AGGREGATE_LONG_DOUBLE_NESTED)
struct S { struct { long double x[2]; } nested; };
int take(struct S s) { return (int)s.nested.x[0]; }
int main(void) { struct S s = {{{3, 4}}}; return take(s); }
#elif defined(REJECT_AGGREGATE_LONG_DOUBLE_UNION)
union U { long x; long double y; };
union U make(void) { union U u = {3}; return u; }
int main(void) { return (int)make().x; }
#elif defined(REJECT_VARARG_LONG_DOUBLE)
int printf(const char *format, ...);
int main(void) { return printf("%.1Lf", 1.0L); }
#elif defined(REJECT_UNPROTOTYPED_AGGREGATE)
struct S { int n; };
int external();
int main(void) { struct S s = {3}; return external(s); }
#elif defined(REJECT_PPC_VARIADIC)
typedef struct Item {} case {} Item;
int pick<Item *p>(int n, ...) { return n; }
int main(void) { Item item; return pick<&item>(1, 2); }
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
