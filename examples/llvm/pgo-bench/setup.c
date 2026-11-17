#include "../pgo/variants.h"

static Shape.rectangle rectangle = {0, 10.0};
static Shape.square square = {0, 3.0};

// Called once per process, outside the timed region. Dispatch and handlers
// are the same objects as in llvm-pgo-demo; setup stays in a separate object.
Shape *benchmark_prepare(int mode, Shape **shapes) {
  shapes[0] = shapes[1] = &rectangle;
  if (mode == 1)
    shapes[0] = shapes[1] = &square;
  if (mode == 2)
    shapes[1] = &square;
  if (mode == 3) {
    Shape *extra = extension_shape(3);
    shapes[0] = extra;
    return extra;
  }
  return 0;
}

long benchmark_visits(Shape *shape) { return shape ? shape->visits : 0; }
