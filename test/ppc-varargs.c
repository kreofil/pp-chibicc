#include <stdarg.h>
#include <stdio.h>

typedef struct Value { int id; } case { int number; } Value;

static double sum_values(va_list ap, int n) {
  double sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, double);
  return sum;
}

double total<Value *v>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  double sum = sum_values(ap, n);
  va_end(ap);
  return v->id + sum;
}
double total<Value.number *v>(int n, ...) {
  va_list ap, copy;
  va_start(ap, n);
  va_copy(copy, ap);
  double sum = sum_values(copy, n);
  va_end(copy);
  va_end(ap);
  return v->id + v->number + sum;
}

long pair_values<Value *a, Value *b>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long sum = a->id + b->id;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return sum;
}
long pair_values<Value.number *a, Value.number *b>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long sum = a->number + b->number;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return sum;
}

typedef struct { long id, count, sum; } Gathered;
Gathered gather<Value *v>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return (Gathered){v->id, n, sum};
}

typedef struct Choice { int id; } enum { one } const Choice;
long chosen<Choice *c>(int n, ...) = 0;
long chosen<Choice.void *c>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int value = va_arg(ap, int);
  va_end(ap);
  return c->id + value;
}
long chosen<Choice.one *c>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int value = va_arg(ap, int);
  va_end(ap);
  return c->id + value + 100;
}

int main() {
  Value base = {3};
  Value.number a = {4, 5}, b = {6, 7};
  Value *pointer = &a;
  if (total<&base>(2, 1.25, 2.5) != 9.25 || total<pointer>(2, 1.25f, 2.5) != 15.25)
    return 1;
  if (total<pointer>(10, 1., 2., 3., 4., 5., 6., 7., 8., 9., 10.) != 394.0)
    return 2;
  if (pair_values<&a, &b>(8, 1, 2, 3, 4, 5, 6, 7, 8) != 216 ||
      pair_values<&a, &base>(8, 1, 2, 3, 4, 5, 6, 7, 8) != 211)
    return 3;
  Gathered result = gather<pointer>(8, 1, 2, 3, 4, 5, 6, 7, 8);
  if (result.id != 4 || result.count != 8 || result.sum != 204) return 4;
  Choice zero = {3};
  Choice.one choice = {7};
  if (chosen<&zero>(1, 10) != 13 || chosen<&choice>(1, 10) != 117) return 5;
  puts("PPC varargs OK");
  return 0;
}
