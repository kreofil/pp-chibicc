#include <stdio.h>
#include <stdlib.h>
#include "api.h"
#include "rectangle.h"
#include "square.h"

double probe_open(int count) {
  OpenFigure.rectangle rectangle = {{2.5, 4.0}};
  OpenFigure.square square = 3.5;
  OpenFigure *figures[] = {&rectangle, &square};
  double total = 0.0;
  for (int i = 0; i < count; i++) {
    OpenFigure *f = figures[i % 2];
    total += open_area<f>();
  }
  return total;
}

int main(int argc, char **argv) {
  int count = argc > 1 ? atoi(argv[1]) : 4;
  if (count < 0 || count > 32)
    return 2;
  printf("n=%d known=%.2f closed=%.2f open=%.2f extra=%.2f control=%.2f multi=%.2f\n", count,
         probe_known(count), probe_closed(count), probe_open(count), extra_probe(count),
         probe_control(count), probe_multi(count));
  return 0;
}
