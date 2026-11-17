// Instantiate identical C functions in independently compiled ABI modules.
#define LD_CAT_I(a, b) a##_##b
#define LD_CAT(a, b) LD_CAT_I(a, b)
#define LD_FN(name) LD_CAT(LD_PREFIX, name)
#define LD_REMOTE(name) LD_CAT(LD_OTHER, name)
long double LD_FN(scalar)(long double x, int bias, double scale) { return x * scale + bias; }
long double LD_FN(sse_budget)(long double x, double a, double b, double c, double d,
                             double e, double f, LDFpPair pair, double tail) {
  return x + a + b + c + d + e + f + pair.a + pair.b + tail;
}
long double LD_FN(gp_budget)(long double x, long a, long b, long c, long d,
                            LDIntPair pair, long tail) {
  return x + a + b + c + d + pair.a + pair.b + tail;
}
long double LD_FN(box_budget)(LDBox x, double a, double b, double c, double d,
                             double e, double f, LDFpPair pair) {
  return x.x + a + b + c + d + e + f + pair.a + pair.b;
}
LDBox LD_FN(box)(LDBox x) { x.x += 0.5L; return x; }
LDNested LD_FN(nested)(LDNested x) { x.inner.items[0] *= 2; return x; }
LDX87 LD_FN(x87)(LDX87 x) { x.y += 0.75L; return x; }
LDMixed LD_FN(mixed)(LDMixed x) { x.x += 1.25L; return x; }
LDFloatUnion LD_FN(float_union)(LDFloatUnion x) { x.x += 2.5L; return x; }
LDIntegerUnion LD_FN(integer_union)(LDIntegerUnion x) { x.words[0] += 3; x.words[1] += 4; return x; }
LDLarge LD_FN(large)(LDLarge x) { x.items[0] += 1; x.items[1] *= 2; x.tag++; return x; }
LDPacked LD_FN(packed)(LDPacked x) { x.x += x.tag; x.tag++; return x; }
long double LD_FN(consume)(int n, va_list ap) {
  long double sum = 0;
  for (int i = 1; i <= n; i++) {
    int integer = va_arg(ap, int);
    double real = va_arg(ap, double);
    long double wide = va_arg(ap, long double);
    sum += i * (integer + real + wide);
  }
  return sum;
}
long double LD_FN(variadic)(long double prefix, int n, ...) {
  va_list ap, copy;
  va_start(ap, n);
  va_copy(copy, ap);
  long double sum = LD_FN(consume)(n, ap) + LD_FN(consume)(n, copy);
  va_end(copy);
  va_end(ap);
  return sum + prefix;
}
long double LD_FN(forward)(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long double sum = LD_REMOTE(consume)(n, ap);
  va_end(ap);
  return sum;
}
LDBox LD_FN(variadic_box)(int n, ...) {
  va_list ap;
  va_start(ap, n);
  LDBox result = {0};
  for (int i = 0; i < n; i++) result.x += va_arg(ap, long double);
  va_end(ap);
  return result;
}
LDLarge LD_FN(variadic_large)(int n, ...) {
  va_list ap;
  va_start(ap, n);
  LDLarge result = {{0, 0}, n};
  for (int i = 0; i < n; i++) result.items[i % 2] += va_arg(ap, long double);
  va_end(ap);
  return result;
}
#undef LD_REMOTE
#undef LD_FN
#undef LD_CAT
#undef LD_CAT_I
#undef LD_PREFIX
#undef LD_OTHER
