#if defined(REJECT_FLOAT)
int main(void) { double x = 1.5; return (int)x; }
#elif defined(REJECT_STRUCT)
int main(void) { struct S { int x; } s = {3}; return s.x; }
#elif defined(REJECT_PPC)
struct Figure {} case { int circle; };
int main(void) { struct Figure.circle c = 3; return c.circle; }
#elif defined(REJECT_VARIADIC)
int printf(const char *format, ...);
int main(void) { return printf("%d", 1); }
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
