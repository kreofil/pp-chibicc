#include "api.h"
#include "base.h"

OpenFigure + { struct { double base, height; } triangle; };
double open_area<OpenFigure.triangle *f>(void) {
  return 0.5 * f->triangle.base * f->triangle.height;
}

double extra_probe(int count) {
  OpenFigure.triangle triangle = {{3.0, 4.0}};
  double total = 0.0;
  for (int i = 0; i < count; i++)
    total += open_area<&triangle>();
  return total;
}
