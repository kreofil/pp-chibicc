#define main ppc_long_double_common_main
#include "../../ppc-long-double.c"
#undef main

typedef struct { long double value; } ExtendedBox;
ExtendedBox boxed<Measure *m>(ExtendedBox previous);
ExtendedBox boxed<Measure *m>(ExtendedBox previous) { previous.value += m->id; return previous; }
ExtendedBox boxed<Measure.wide *m>(ExtendedBox previous) {
  previous.value += m->wide.value;
  return previous;
}
typedef union { long double value; long integer; } ExtendedUnion;
ExtendedUnion union_result<Measure *m>(ExtendedUnion previous);
ExtendedUnion union_result<Measure *m>(ExtendedUnion previous) { previous.value += m->id; return previous; }
ExtendedUnion union_result<Measure.wide *m>(ExtendedUnion previous) {
  previous.value += m->wide.value;
  return previous;
}
int main(void) {
  if (ppc_long_double_common_main()) return 1;
  Measure zero = {3};
  Measure *objects[] = {&wide_measure, &zero};
  ExtendedBox box = {0.5L};
  ExtendedBox result = boxed<objects[0]>(box);
  ExtendedBox zero_box = boxed<objects[1]>(box);
  if (result.value != wide_measure.wide.value + 0.5L || zero_box.value != 3.5L)
    return 2;
  ExtendedUnion previous = {0.5L};
  ExtendedUnion next = union_result<objects[0]>(previous);
  ExtendedUnion zero_union = union_result<objects[1]>(previous);
  if (next.value != wide_measure.wide.value + 0.5L ||
      zero_union.value != 3.5L || previous.value != 0.5L)
    return 3;
  return 0;
}
