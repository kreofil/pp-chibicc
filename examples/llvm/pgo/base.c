#include "base.h"

double compute<Shape *shape>(void) { return 0.0; }

double dispatch_many(Shape **shapes, int count) {
  double sum = 0.0;
  for (int i = 0; i < count; i++) {
    Shape *shape = shapes[i % 2];
    sum += compute<shape>();
  }
  return sum;
}
