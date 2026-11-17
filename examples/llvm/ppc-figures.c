#include <stdio.h>

typedef struct Figure { int id; } case {
  struct { int width, height; } rectangle;
  int square;
} Figure;

int Perimeter<Figure *f>(void) { return 0; }
int Perimeter<Figure.rectangle *f>(void) {
  return 2 * (f->rectangle.width + f->rectangle.height);
}
int Perimeter<Figure.square *f>(void) { return 4 * f->square; }

int main(void) {
  Figure.rectangle rectangle = {1, {3, 4}};
  Figure.square square = {2, 5};
  Figure *figures[] = {&rectangle, &square};
  for (int i = 0; i < 2; i++) {
    Figure *f = figures[i];
    printf("figure %d: perimeter=%d\n", f->id, Perimeter<f>());
  }
  return 0;
}
