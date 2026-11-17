#include <stdio.h>
#include <stdlib.h>

typedef struct Shape { int id; } case {
  struct { int x, y; } rectangle;
  int square;
} Shape;

int measure<Shape *s>(int extra) { return s->id + extra; }
int measure<Shape.rectangle *s>(int extra) {
  return s->rectangle.x * s->rectangle.y + s->id + extra;
}
int measure<Shape.square *s>(int extra) { return s->square * s->square + extra; }
int measure<Shape.void *s>(int extra) { return s->id - extra; }

int combine<Shape *a, Shape *b>(int n) { return n; }
int combine<Shape.rectangle *a, Shape.square *b>(int n) {
  return a->rectangle.x + b->square + n;
}

typedef struct Choice { int value; } enum { first, second } const Choice;
int choose<Choice *c>(void) = 0;
int choose<Choice.void *c>(void) { return c->value; }
int choose<Choice.first *c>(void) { return c->value + 10; }
int choose<Choice.second *c>(void) { return c->value + 20; }

Shape.rectangle global_shape = {7, {2, 6}};
Choice global_choice = Choice.second;

Shape.rectangle copy_rectangle(Shape.rectangle value) {
  value.id++;
  return value;
}

int main(void) {
  Shape.rectangle r = {10, {3, 4}};
  Shape.square s = {20, 5};
  Shape zero = {9};
  Shape *p = &r;
  if (measure<p>(5) != 27 || measure<&s>(3) != 28 || measure<&zero>(2) != 7)
    return 1;
  if (measure<&global_shape>(1) != 20 || get_spec_size(Shape) != 3 ||
      get_spec_var_index(p) != get_subtype_index(Shape.rectangle))
    return 2;
  Shape.rectangle copy = r;
  if (measure<&copy>(0) != 22 || combine<&copy, &s>(8) != 16 ||
      combine<&s, &copy>(8) != 8)
    return 3;
  Shape.rectangle *dynamic = create_spec(Shape.rectangle, 2);
  dynamic[0].id = 1;
  dynamic[0].rectangle.x = 2;
  dynamic[0].rectangle.y = 3;
  if (measure<dynamic>(0) != 7 ||
      get_spec_var_index(dynamic + 1) != get_subtype_index(Shape.rectangle))
    return 4;
  free(dynamic);
  Choice.first first = {3};
  Choice.second second = {4};
  Choice values[2] = {first, second};
  if (choose<&values[0]>() != 13)
    return 5;
  if (choose<&values[1]>() != 24 || choose<&global_choice>() != 20)
    return 6;
  Shape.rectangle returned = copy_rectangle(r);
  if (measure<&returned>(0) != 23 || r.id != 10 ||
      get_spec_var_index(&returned) != get_subtype_index(Shape.rectangle))
    return 7;
  printf("PPC runtime OK\n");
  return 0;
}
