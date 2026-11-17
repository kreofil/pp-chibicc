#ifndef LLVM_FLOATING_ABI_H
#define LLVM_FLOATING_ABI_H

typedef struct { long a, b; } IntegerPair;
typedef struct { float f; double d; } FloatingStorage;

#define DECLARE_FLOATING_API(prefix) \
  float prefix##_float(float x, int bias); \
  double prefix##_mixed(long a, double b, float c, int d, double e, const int *p); \
  double prefix##_stack(float a, double b, float c, double d, float e, \
                       double f, float g, double h, float i, double j); \
  IntegerPair prefix##_pair(double f, long a, float g, long b, long c, long d, \
                           IntegerPair pair); \
  long prefix##_gp(double a, double b, double c, double d, double e, double f, \
                   IntegerPair pair)

DECLARE_FLOATING_API(host);
DECLARE_FLOATING_API(emitted);
double host_variadic(int count, ...);
int host_calls_floating(void);
extern float host_float_value;
extern double host_double_value;
extern FloatingStorage host_storage;

#endif
