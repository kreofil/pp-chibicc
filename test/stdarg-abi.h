#ifndef TEST_STDARG_ABI_H
#define TEST_STDARG_ABI_H
#include <stdarg.h>

#define VA_INTS 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
#define VA_DOUBLES 1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 11., 12.
#define VA_MIXED 1, 1., 2, 2., 3, 3., 4, 4., 5, 5., 6, 6., 7, 7., 8, 8., 9, 9., 10, 10., 11, 11., 12, 12.
#define VA_FIXED 1, 2, 3, 4, 5, 6, 7, 1., 2., 3., 4., 5., 6., 7., 8., 9., 10

typedef struct { long id; double factor; } VaPrefix;
typedef struct { long n; double value; } VaSmall;
typedef struct { long a, b, c; } VaLarge;

long va_sum_ints(int n, ...);
double va_sum_doubles(int n, ...);
double va_interleaved(int n, ...);
double va_fixed_stack(int a, int b, int c, int d, int e, int f, int g,
                      double x0, double x1, double x2, double x3, double x4,
                      double x5, double x6, double x7, double x8, int marker, ...);
double va_named_record(VaPrefix prefix, int n, ...);
long va_named_memory(VaLarge prefix, int n, ...);
VaSmall va_small_result(int n, ...);
VaLarge va_large_result(int n, ...);
void va_void_result(int n, ...);
double va_consume_doubles(va_list ap, int n);
int va_format(char *buf, const char *text, ...);
#endif
