#include <stdio.h>

typedef struct Rectangle { long width, height; } Rectangle;
typedef struct Report { long kind, width, height; } Report;
typedef struct Figure { long id; } case {
  Rectangle rectangle;
  long square;
} Figure;

Rectangle enlarge(Rectangle value, long extra) {
  value.width += extra;
  value.height += extra;
  return value;
}
Report describe<Figure *f>(Rectangle extra) {
  Report result = {0, extra.width, extra.height};
  return result;
}
Report describe<Figure.rectangle *f>(Rectangle extra) {
  Report result = {1, f->rectangle.width + extra.width,
                     f->rectangle.height + extra.height};
  return result;
}
Report describe<Figure.square *f>(Rectangle extra) {
  Report result = {2, f->square + extra.width, f->square + extra.height};
  return result;
}

int main(void) {
  Rectangle original = {3, 4};
  Rectangle larger = enlarge(original, 1);
  Figure.rectangle rectangle = {7, larger};
  Figure.square square = {8, 6};
  Figure *figures[] = {&rectangle, &square};
  for (int i = 0; i < 2; i++) {
    Report result = describe<figures[i]>(original);
    printf("kind %ld: %ld x %ld\n", result.kind, result.width, result.height);
  }
  return 0;
}
