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

// The default is defined after the concrete handlers and their caller.
double closed_interaction<ClosedFigure.rectangle *a, ClosedFigure.rectangle *b>(void) {
  return a->rectangle.width * b->rectangle.height;
}
double probe_multi(int count) {
  ClosedFigure.rectangle rectangle = {{2.5, 4.0}};
  ClosedFigure.square square = 3.5;
  ClosedFigure *figures[] = {&rectangle, &square};
  double total = 0;
  for (int i = 0; i < count; i++) {
    ClosedFigure *a = figures[i % 2];
    ClosedFigure *b = figures[(i / 2) % 2];
    total += closed_interaction<a, b>();
  }
  return total;
}
double closed_interaction<ClosedFigure.rectangle *a, ClosedFigure.square *b>(void) {
  return a->rectangle.width * b->square;
}
double closed_interaction<ClosedFigure.square *a, ClosedFigure.rectangle *b>(void) {
  return a->square * b->rectangle.height;
}
double closed_interaction<ClosedFigure.square *a, ClosedFigure.square *b>(void) {
  return a->square * b->square;
}
double closed_interaction<ClosedFigure *a, ClosedFigure *b>(void) { return 0; }
