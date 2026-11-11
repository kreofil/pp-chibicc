#include "api.h"

typedef struct ClosedFigure {} case {
  struct { double width, height; } rectangle;
  double square;
} const ClosedFigure;

double closed_area<ClosedFigure *f>(void) = 0;
double closed_area<ClosedFigure.void *f>(void) { return 0.0; }
double closed_area<ClosedFigure.rectangle *f>(void) {
  return f->rectangle.width * f->rectangle.height;
}
double closed_area<ClosedFigure.square *f>(void) { return f->square * f->square; }

double probe_known(int count) {
  ClosedFigure.rectangle rectangle = {{2.5, 4.0}};
  double total = 0.0;
  for (int i = 0; i < count; i++)
    total += closed_area<&rectangle>();
  return total;
}

double probe_closed(int count) {
  ClosedFigure.rectangle rectangle = {{2.5, 4.0}};
  ClosedFigure.square square = 3.5;
  ClosedFigure *figures[] = {&rectangle, &square};
  double total = 0.0;
  for (int i = 0; i < count; i++) {
    ClosedFigure *f = figures[i % 2];
    total += closed_area<f>();
  }
  return total;
}

static double rectangle_control(double width, double height) { return width * height; }

double probe_control(int count) {
  double (*handler)(double, double) = rectangle_control;
  double total = 0.0;
  for (int i = 0; i < count; i++)
    total += handler(2.5, 4.0);
  return total;
}
