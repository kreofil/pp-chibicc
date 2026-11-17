#include <stdio.h>

typedef struct Rectangle { double width, height; } Rectangle;
typedef struct Report { long kind; double area; } Report;
typedef struct Figure {} case {
  Rectangle rectangle;
  double square;
} Figure;

Rectangle enlarge(Rectangle value, double extra) {
  value.width += extra;
  value.height += extra;
  return value;
}
Report describe<Figure *f>(Rectangle extra) {
  Report result = {0, 0.0};
  return result;
}
Report describe<Figure.rectangle *f>(Rectangle extra) {
  Report result = {1, (f->rectangle.width + extra.width) *
                     (f->rectangle.height + extra.height)};
  return result;
}
Report describe<Figure.square *f>(Rectangle extra) {
  Report result = {2, (f->square + extra.width) * (f->square + extra.height)};
  return result;
}

int main(void) {
  Rectangle original = {2.5, 3.0};
  Rectangle larger = enlarge(original, 0.5);
  Figure.rectangle rectangle = {larger};
  Figure.square square = 3.5;
  Figure *figures[] = {&rectangle, &square};
  for (int i = 0; i < 2; i++) {
    Report result = describe<figures[i]>(original);
    printf("kind %ld: area=%.2f\n", result.kind, result.area);
  }
  return original.width != 2.5 || original.height != 3.0;
}
