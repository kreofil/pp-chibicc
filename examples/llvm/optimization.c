#include <stdio.h>
#include <stdlib.h>

typedef struct Pair { double x, y; } Pair;
typedef struct Figure {} case {
  Pair rectangle;
  double square;
} Figure;

// Public functions keep inspectable bodies even when O2 inlines their calls.
Pair grow(Pair value, double step) {
  value.x += step;
  value.y += step;
  return value;
}

double local_sum(int count, Pair start) {
  Pair current = start;
  double total = 0.0;
  for (int i = 0; i < count; i++) {
    current = grow(current, 0.5);
    total += current.x + current.y;
  }
  return total;
}

double Area<Figure *f>(Pair extra) { return 0.0; }
double Area<Figure.rectangle *f>(Pair extra) {
  return (f->rectangle.x + extra.x) * (f->rectangle.y + extra.y);
}
double Area<Figure.square *f>(Pair extra) {
  return (f->square + extra.x) * (f->square + extra.y);
}

double dispatch_sum(Figure **figures, int count, Pair extra) {
  double total = 0.0;
  for (int i = 0; i < count; i++) {
    Figure *f = figures[i % 2];
    total += Area<f>(extra);
  }
  return total;
}

int main(int argc, char **argv) {
  int count = argc > 1 ? atoi(argv[1]) : 4;
  if (count < 0 || count > 32)
    return 2;
  Pair start = {1.5, 2.5};
  Pair extra = {0.5, 1.0};
  Figure.rectangle rectangle = {{2.5, 4.0}};
  Figure.square square = 3.5;
  Figure *figures[] = {&rectangle, &square};
  printf("n=%d locals=%.2f dispatch=%.2f\n", count,
         local_sum(count, start), dispatch_sum(figures, count, extra));
  return 0;
}
