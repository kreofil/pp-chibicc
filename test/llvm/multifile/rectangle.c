#include "rectangle.h"
int perimeter<Figure.rectangle *f>(void) {
  return 2 * (f->rectangle.width + f->rectangle.height);
}
Report report<Figure.rectangle *f>(Report previous) {
  previous.id = f->id;
  previous.perimeter = 2 * (f->rectangle.width + f->rectangle.height);
  previous.count += 10;
  return previous;
}
