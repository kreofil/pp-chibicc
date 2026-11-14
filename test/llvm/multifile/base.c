#include "base.h"
#include <stdarg.h>
long variable_sum<Figure *f>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long sum = f->id;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return sum;
}
int perimeter<Figure *f>(void) { return f->id; }
int pair<Figure *first, Figure *second>(int extra) { return extra; }
Report report<Figure *f>(Report previous) {
  previous.id = f->id;
  previous.perimeter = perimeter<f>();
  previous.count++;
  return previous;
}
Measurement measure<Figure *f>(Dimensions extra) {
  Measurement result = {f->id, extra.width + extra.height};
  return result;
}

long double extended<Figure *f>(long double extra) { return f->id + extra; }

int closed_pick<MFClosedChoice.void *x>(void) { return 10; }
int closed_pair<MFClosedChoice *a, MFClosedUnion *b>(void) { return 20; }
int closed_sizes(void) {
  enum { all = get_spec_size(MFClosedCase) + get_spec_size(MFClosedChoice) +
               get_spec_size(MFClosedPointer) + get_spec_size(MFClosedUnion) };
  return all;
}
