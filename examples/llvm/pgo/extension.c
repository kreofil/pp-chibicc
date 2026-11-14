#include "variants.h"

Shape + { double growing; double switching; };
static Shape.growing growing = {0, 2.0};
static Shape.switching switching = {0, 2.0};

double compute<Shape.growing *shape>(void) {
  shape->visits++;
  shape->growing += 0.5;
  return shape->growing;
}

double compute<Shape.switching *shape>(void) {
  shape->visits++;
  shape->switching += 0.5;
  double result = shape->switching;
  // Both specializations have the same layout. Preserve payload and common
  // fields, and select the existing rectangle handler for later calls.
  init_spec(Shape.rectangle, shape);
  return result;
}

Shape *extension_shape(int mode) {
  return mode == 3 ? (Shape *)&growing : (Shape *)&switching;
}
