#include "rectangle.h"
#include <stdarg.h>
long variable_sum<Figure.rectangle *f>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long sum = f->id + 100;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return sum;
}
int perimeter<Figure.rectangle *f>() {
  return 2 * (f->rectangle.width + f->rectangle.height);
}
Report report<Figure.rectangle *f>(Report previous) {
  previous.id = f->id;
  previous.perimeter = 2 * (f->rectangle.width + f->rectangle.height);
  previous.count += 10;
  return previous;
}
Measurement measure<Figure.rectangle *f>(Dimensions extra) {
  Measurement result = {f->id, (f->rectangle.width + extra.width) *
                              (f->rectangle.height + extra.height)};
  return result;
}

long double extended<Figure.rectangle *f>(long double extra) { return f->rectangle.width * f->rectangle.height + extra; }

int closed_pick<MFClosedChoice.first *x>(void) {
  switch (get_spec_var_index(x)) {
    case get_subtype_index(MFClosedChoice.first): return 11 + x->id;
    default: return -1;
  }
}
int closed_pair<MFClosedChoice.first *a, MFClosedUnion.second *b>(void) { return 32; }
