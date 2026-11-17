#include "families.h"
unsigned long bench_short2<BenchLeft *a, BenchRight *b>(unsigned long x, BenchStats *s) {
  s->defaults++;
  unsigned code = 37 + 3 * get_spec_var_index(a) + get_spec_var_index(b);
  return (x + 1) * code + 13;
}
unsigned long bench_work2<BenchLeft *a, BenchRight *b>(unsigned long x, BenchStats *s) {
  s->defaults++;
  return bench_mix(x, 37 + 3 * get_spec_var_index(a) + get_spec_var_index(b));
}
unsigned long bench_short3<BenchLeft *a, BenchRight *b, BenchThird *c>(unsigned long x, BenchStats *s) {
  s->defaults++;
  unsigned code = 37 + 6 * get_spec_var_index(a) + 2 * get_spec_var_index(b) + get_spec_var_index(c);
  return (x + 1) * code + 13;
}
unsigned long bench_work3<BenchLeft *a, BenchRight *b, BenchThird *c>(unsigned long x, BenchStats *s) {
  s->defaults++;
  return bench_mix(x, 37 + 6 * get_spec_var_index(a) + 2 * get_spec_var_index(b) + get_spec_var_index(c));
}
