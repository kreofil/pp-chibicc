#include "base.h"
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
