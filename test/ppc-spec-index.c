#include "test.h"

typedef struct IndexFigure { int common; } case {
  default int base;
  int rect;
  void marker;
} IndexFigure;

typedef struct IndexMode {} enum { Off, On } IndexMode;

static int ptr_calls;

static IndexFigure *once(IndexFigure *p) {
  ptr_calls++;
  return p;
}

int main() {
  ASSERT(0, get_subtype_index(IndexFigure));
  ASSERT(0, get_subtype_index(IndexFigure.base));

  int rect_idx = get_subtype_index(IndexFigure.rect);
  int marker_idx = get_subtype_index(IndexFigure.marker);
  ASSERT(1, rect_idx > 0);
  ASSERT(1, marker_idx > 0);
  ASSERT(1, rect_idx != marker_idx);

  IndexFigure general;
  IndexFigure.base base;
  IndexFigure.rect rect;
  IndexFigure.marker marker;

  ASSERT(0, get_spec_var_index(&general));
  ASSERT(0, get_spec_var_index(&base));
  ASSERT(rect_idx, get_spec_var_index(&rect));
  ASSERT(marker_idx, get_spec_var_index(&marker));

  IndexFigure *p = &rect;
  ASSERT(rect_idx, get_spec_var_index(p));

  ptr_calls = 0;
  ASSERT(rect_idx, get_spec_var_index(once(p)));
  ASSERT(1, ptr_calls);

  int off_idx = get_subtype_index(IndexMode.Off);
  int on_idx = get_subtype_index(IndexMode.On);
  ASSERT(0, get_subtype_index(IndexMode));
  ASSERT(1, off_idx > 0);
  ASSERT(1, on_idx > 0);
  ASSERT(1, off_idx != on_idx);

  IndexMode mode = IndexMode.On;
  ASSERT(on_idx, get_spec_var_index(&mode));
  mode = IndexMode.Off;
  ASSERT(off_idx, get_spec_var_index(&mode));

  printf("OK\n");
  return 0;
}
