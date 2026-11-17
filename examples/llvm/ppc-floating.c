#include <stdio.h>

typedef struct Figure { int id; } case {
  struct { double width, height; } rectangle;
  double square;
} Figure;

double Area<Figure *f>(double scale) { return 0.0; }
double Area<Figure.rectangle *f>(double scale) {
  return f->rectangle.width * f->rectangle.height * scale * scale;
}
double Area<Figure.square *f>(double scale) {
  return f->square * f->square * scale * scale;
}

int main(void) {
  Figure.rectangle rectangle = {1, {2.5, 4.0}};
  Figure.square square = {2, 3.5};
  Figure *figures[] = {&rectangle, &square};
  for (int i = 0; i < 2; i++) {
    Figure *f = figures[i];
    printf("figure %d: area=%.2f, scaled=%.2f\n", f->id,
           Area<f>(1.0), Area<f>(2.0));
  }
  return 0;
}
