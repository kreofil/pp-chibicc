#include <stdlib.h>
#include <string.h>
#include "ppvector.h"
#include "figure-rectangle.h"
#include "figure-triangle.h"
#include "api.h"

ppVector + { Figure *figure_ptr; };
double FigurePerimeter<Figure *figure>(void);

static ppVector.figure_ptr figures;
static Figure.rect *rectangles;
static Figure.trian *triangles;
static Figure **original;
static int expected_count;
static int valid;

static int compare_perimeters(const void *left, const void *right) {
  Figure *a = *(Figure **)left;
  Figure *b = *(Figure **)right;
  double pa = FigurePerimeter<a>();
  double pb = FigurePerimeter<b>();
  return (pa > pb) - (pa < pb);
}

void ppVector_sort<ppVector.figure_ptr *vector>(void) {
  if (ppVector_size((ppVector *)vector) > 1)
    qsort(ppVector_data((ppVector *)vector), ppVector_size((ppVector *)vector),
          sizeof(Figure *), compare_perimeters);
}

int figures_prepare(int count) {
  if (count < 0 || count > 32768) return 0;
  expected_count = count;
  valid = 1;
  ppVector_init((ppVector *)&figures);
  init_spec(ppVector.figure_ptr, &figures);
  figures.foundation_size = sizeof(Figure *);
  figures.foundation_addr = &figures.figure_ptr;
  if (!count) return 1;
  rectangles = calloc(count, sizeof(Figure.rect));
  triangles = calloc(count, sizeof(Figure.trian));
  original = calloc(count, sizeof(Figure *));
  ppVector_reserve((ppVector *)&figures, count);
  return rectangles && triangles && original && figures.vec_memory;
}

int figures_add(int kind, int a, int b, int c) {
  unsigned int index = ppVector_size((ppVector *)&figures);
  if (index >= expected_count || (kind != 1 && kind != 2)) return 0;
  Figure *shape;
  if (kind == 1) {
    Figure.rect *r = &rectangles[index];
    init_spec(Figure.rect, r);
    r->rect.x = a;
    r->rect.y = b;
    shape = (Figure *)r;
  } else {
    Figure.trian *t = &triangles[index];
    init_spec(Figure.trian, t);
    t->trian.a = a;
    t->trian.b = b;
    t->trian.c = c;
    shape = (Figure *)t;
  }
  original[index] = shape;
  ppVector_PUSH_BACK(figures, figure_ptr, shape)
  return 1;
}

unsigned long figures_process(void) {
  unsigned int count = ppVector_size((ppVector *)&figures);
  if (count != expected_count) valid = 0;
  Figure **items = ppVector_data((ppVector *)&figures);
  if (count) memcpy(items, original, count * sizeof(Figure *));
  ppVector_sort<(ppVector *)&figures>();
  double previous = -1.0;
  unsigned long checksum = 0;
  for (unsigned int i = 0; i < count; i++) {
    Figure *shape = items[i];
    double perimeter = FigurePerimeter<shape>();
    if (perimeter < previous) valid = 0;
    previous = perimeter;
    checksum += (unsigned long)perimeter * (i + 1);
  }
  return checksum;
}

int figures_valid(void) { return valid; }

void figures_destroy(void) {
  ppVector_destroy((ppVector *)&figures);
  free(original);
  free(rectangles);
  free(triangles);
}
