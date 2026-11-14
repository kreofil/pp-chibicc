#include <stdarg.h>
int puts(const char *s);

typedef struct Measure { int id; } case { struct { long double value; } wide; } Measure;
long double evaluate<Measure *m>(long double offset, double factor);
long double evaluate<Measure *m>(long double offset, double factor) { return offset + m->id; }
long double evaluate<Measure.wide *m>(long double offset, double factor) {
  return m->wide.value * factor + offset;
}
long double collect<Measure *m>(int n, ...);
long double collect<Measure *m>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long double sum = m->id;
  for (int i = 0; i < n; i++) sum += va_arg(ap, long double);
  va_end(ap);
  return sum;
}
long double collect<Measure.wide *m>(int n, ...) {
  va_list ap, copy;
  va_start(ap, n);
  va_copy(copy, ap);
  long double sum = m->wide.value;
  for (int i = 0; i < n; i++) sum += va_arg(ap, long double) + va_arg(copy, long double);
  va_end(copy);
  va_end(ap);
  return sum;
}
typedef struct { int id; long double value; } MeasureReport;
MeasureReport measure_report<Measure *m>(MeasureReport previous);
MeasureReport measure_report<Measure *m>(MeasureReport previous) {
  previous.id = m->id;
  previous.value += m->id;
  return previous;
}
MeasureReport measure_report<Measure.wide *m>(MeasureReport previous) {
  previous.id = m->id;
  previous.value += m->wide.value;
  return previous;
}
Measure.wide wide_measure = {7, {0x1.0000000000000002p0L}};
int main(void) {
  Measure zero = {3};
  Measure *objects[] = {&wide_measure, &zero};
  if (evaluate<objects[0]>(0.0L, 1.0) - 1.0L != 0x1p-63L) return 1;
  if (evaluate<objects[0]>(0.5L, 2.0) != wide_measure.wide.value * 2 + 0.5L) return 2;
  if (evaluate<objects[1]>(1.25L, 9.0) != 4.25L) return 3;
  if (collect<objects[0]>(2, 1.25L, 2.5L) != wide_measure.wide.value + 7.5L) return 4;
  if (collect<objects[1]>(2, 1.25L, 2.5L) != 6.75L) return 5;
  MeasureReport previous = {0, 0.5L};
  MeasureReport measured = measure_report<objects[0]>(previous);
  MeasureReport zero_report = measure_report<objects[1]>(previous);
  if (measured.id != 7 || measured.value != wide_measure.wide.value + 0.5L ||
      previous.value != 0.5L || zero_report.value != 3.5L)
    return 6;
  puts("PPC long double OK");
  return 0;
}
