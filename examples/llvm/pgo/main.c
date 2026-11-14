#include <stdio.h>
#include <stdlib.h>
#include "variants.h"

int main(int argc, char **argv) {
  int mode = argc > 1 ? atoi(argv[1]) : 2;
  int count = argc > 2 ? atoi(argv[2]) : 7;
  if (mode < 0 || mode > 5 || count < 0 || count > 4096)
    return 2;
  Shape.rectangle rectangle = {0, 10.0};
  Shape.square square = {0, 3.0};
  Shape zero = {0};
  Shape *extra = 0;
  Shape *shapes[] = {&rectangle, &rectangle};
  if (mode == 1)
    shapes[0] = shapes[1] = &square;
  if (mode == 2)
    shapes[1] = &square;
  if (mode == 3 || mode == 4) {
    extra = extension_shape(mode);
    if (!extra)
      return 3;
    shapes[0] = extra;
  }
  if (mode == 5)
    shapes[0] = &zero;
  double sum = dispatch_many(shapes, count);
  if (rectangle.visits || square.visits || zero.visits)
    return 4;
  int retagged = extra && get_spec_var_index(extra) == get_subtype_index(Shape.rectangle);
  printf("mode=%d n=%d sum=%.2f visits=%ld retagged=%d\n", mode, count, sum,
         extra ? extra->visits : 0L, retagged);
  return 0;
}
