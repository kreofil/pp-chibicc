#include <stdarg.h>
int puts(const char *);
int printf(const char *, ...);
#define CHECK(x) do { if (!(x)) { printf("PPC bitfield failure: %d\n",__LINE__); return 1; } } while (0)
typedef struct Flags { unsigned level:3; int delta:5; _Bool ready:1; } Flags;
typedef struct BitChoice { unsigned base:3; } case { Flags flags; } const BitChoice;
static BitChoice.flags global={5,{7,-3,1}};
typedef struct ScalarBits {} case {
  default unsigned fallback:2;
  unsigned code:3;
} const ScalarBits;
int bit_scalar<ScalarBits *v>() { return 99; }
int bit_scalar<ScalarBits.fallback *v>() { return v->fallback; }
int bit_scalar<ScalarBits.code *v>() { return v->code; }
Flags bit_pick<BitChoice *v>(Flags input) { input.level+=v->base; return input; }
Flags bit_pick<BitChoice.flags *v>(Flags input) {
  input.level+=v->flags.level; input.delta+=v->flags.delta;
  input.ready=input.ready && v->flags.ready; return input;
}
int bit_pair<BitChoice *a, BitChoice *b>() { return a->base+b->base; }
int bit_pair<BitChoice.flags *a, BitChoice.flags *b>() {
  return a->flags.level+b->flags.level+a->flags.delta+b->flags.delta;
}
int bit_variadic<BitChoice *v>(int count, ...) {
  va_list ap; va_start(ap,count);
  int sum=v->base;
  for (int i=0;i<count;i++) sum+=va_arg(ap,int);
  va_end(ap); return sum;
}
int main(void) {
  ScalarBits.code code={7};
  ScalarBits.fallback fallback={3};
  CHECK(bit_scalar<&code>()==7 && bit_scalar<&fallback>()==3);
  BitChoice base={3};
  BitChoice.flags chosen={6,{2,-5,1}};
  Flags initial={7,15,1};
  Flags a=bit_pick<&base>(initial);
  Flags b=bit_pick<&chosen>(initial);
  CHECK(a.level==2 && a.delta==15 && a.ready==1);
  CHECK(b.level==1 && b.delta==10 && b.ready==1);
  Flags g=bit_pick<&global>(initial);
  CHECK(g.level==6);
  CHECK((bit_pair<&base,&chosen>()==9));
  CHECK((bit_pair<&global,&chosen>()==1));
  CHECK(bit_variadic<&base>(3,chosen.flags.level,chosen.flags.delta,chosen.flags.ready)==1);
  int calls=0;
  Flags evaluated=bit_pick<(calls++,&chosen)>(initial);
  CHECK(evaluated.delta==10 && calls==1);
  unsigned n=31;
  CHECK((chosen.flags.level=n)==7 && chosen.flags.level++==7 && chosen.flags.level==0);
  CHECK(code.code++==7 && bit_scalar<&code>()==0);
  switch (chosen.flags.level) { case 0: break; default: return 2; }
  puts("PPC bitfield OK");
  return 0;
}
