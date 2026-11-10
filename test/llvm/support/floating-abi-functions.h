// Compile the same operations on both sides of the ABI, with separate names.
#define NAME_(prefix, suffix) prefix##suffix
#define NAME(prefix, suffix) NAME_(prefix, suffix)

float NAME(FP_PREFIX, _float)(float x, int bias) { return x + bias; }
double NAME(FP_PREFIX, _mixed)(long a, double b, float c, int d, double e,
                             const int *p) {
  return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * *p;
}
double NAME(FP_PREFIX, _stack)(float a, double b, float c, double d, float e,
                             double f, float g, double h, float i, double j) {
  return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g +
         8 * h + 9 * i + 10 * j;
}
IntegerPair NAME(FP_PREFIX, _pair)(double f, long a, float g, long b, long c,
                                 long d, IntegerPair pair) {
  pair.a += a + b + (long)f;
  pair.b += c + d + (long)g;
  return pair;
}
long NAME(FP_PREFIX, _gp)(double a, double b, double c, double d, double e,
                        double f, IntegerPair pair) {
  return (long)(a + b + c + d + e + f) + pair.a * 10 + pair.b;
}

#undef NAME
#undef NAME_
