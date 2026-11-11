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
