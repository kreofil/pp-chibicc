// Compile identical C bodies independently to check both sides of the ABI.
// ABI_FN supplies the host_ or emitted_ prefix in each translation unit.
One ABI_FN(one)(One x) { x.value++; return x; }
Three ABI_FN(three)(Three x) { x.data[0]++; x.data[2]--; return x; }
Twelve ABI_FN(twelve)(Twelve x) { x.c += x.a; return x; }
Pair ABI_FN(pair)(Pair x) { x.a += 3; x.b -= 2; return x; }
Large ABI_FN(large)(Large x, long n) { x.a += n; x.c = x.a + x.b; return x; }
Packed ABI_FN(packed)(Packed x, long n) { x.value += n; return x; }
Aligned ABI_FN(aligned)(Aligned x, long n) { x.value += n; return x; }
Nested ABI_FN(nested)(Nested x) { x.head++; x.inner.y += 2; x.tail += 3; return x; }
Value ABI_FN(value)(Value x) { x.bytes[0]++; x.bytes[11] += 2; return x; }
PointerPair ABI_FN(pointer)(PointerPair x) { x.pointer += x.offset; return x; }

long ABI_FN(pressure)(long a, long b, long c, long d, long e, Pair p, long last) {
  return a + 2*b + 3*c + 4*d + 5*e + 7*p.a + 11*p.b + 13*last;
}
long ABI_FN(boundary)(long a, long b, long c, long d, Pair p, long last) {
  return a + 2*b + 3*c + 4*d + 7*p.a + 11*p.b + 13*last;
}
Large ABI_FN(sret_pressure)(long a, long b, long c, long d, Pair p, long last) {
  Large result = {a + b + c + d, p.a + p.b, last};
  return result;
}
long ABI_FN(many)(Pair a, Pair b, Pair c, Pair d, long last) {
  return a.a + b.b + c.a + d.b + last;
}
Pair ABI_FN(select)(int which, Pair a, Pair b) { return which ? a : b; }
Large ABI_FN(compose)(Pair a, Large b, long n) {
  b.a += a.a + n;
  b.c += a.b;
  return b;
}

FloatOne ABI_FN(float_one)(FloatOne x) { x.x += 0.5f; return x; }
FloatPair ABI_FN(float_pair)(FloatPair x) { x.x += 0.5f; x.y -= 0.25f; return x; }
FloatTriple ABI_FN(float_triple)(FloatTriple x) { x.z += x.x; return x; }
FloatQuad ABI_FN(float_quad)(FloatQuad x) { x.v[3] += x.v[0]; return x; }
DoubleOne ABI_FN(double_one)(DoubleOne x) { x.x += 0.25; return x; }
DoublePair ABI_FN(double_pair)(DoublePair x) { x.x += x.y; x.y -= 0.5; return x; }
FloatDouble ABI_FN(float_double)(FloatDouble x) { x.x += 0.5f; x.y += x.x; return x; }
DoubleFloat ABI_FN(double_float)(DoubleFloat x) { x.x += x.y; x.y += 0.5f; return x; }
IntFloat ABI_FN(int_float)(IntFloat x) { x.tag += 2; x.value += 0.5f; return x; }
FloatInt ABI_FN(float_int)(FloatInt x) { x.tag += 3; x.value -= 0.5f; return x; }
IntDouble ABI_FN(int_double)(IntDouble x) { x.tag += 2; x.value += 0.5; return x; }
DoubleInt ABI_FN(double_int)(DoubleInt x) { x.tag += 3; x.value -= 0.5; return x; }
NestedReal ABI_FN(nested_real)(NestedReal x) {
  x.parts[1].v[1] += x.parts[0].v[0];
  return x;
}
RealUnion ABI_FN(real_union)(RealUnion x) { x.f[0] += 0.5f; x.f[1] -= 0.25f; return x; }
RealUnion ABI_FN(real_union_double)(RealUnion x) { x.d += 0.5; return x; }
MixedUnion ABI_FN(mixed_union)(MixedUnion x) { x.d += 2.25; return x; }
WideUnion ABI_FN(wide_union)(WideUnion x) { x.f[2] += x.f[0]; return x; }
PackedReal ABI_FN(packed_real)(PackedReal x, double n) { x.value += n; return x; }
RealTriple ABI_FN(real_triple)(RealTriple x, double n) {
  x.v[0] += n;
  x.v[2] = x.v[0] + x.v[1];
  return x;
}
AlignedReal ABI_FN(aligned_real)(AlignedReal x, float n) { x.value += n; return x; }

double ABI_FN(sse_pressure)(double a, double b, double c, double d, double e,
                            double f, double g, DoublePair p, double last) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 11*p.x + 13*p.y + 17*last;
}
double ABI_FN(sse_boundary)(double a, double b, double c, double d, double e,
                            double f, DoublePair p, double last) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 11*p.x + 13*p.y + 17*last;
}
double ABI_FN(vector_boundary)(float a, float b, float c, float d, float e,
                               float f, float g, FloatPair p, float last) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 11*p.x + 13*p.y + 17*last;
}
double ABI_FN(vector_pressure)(float a, float b, float c, float d, float e,
                               float f, float g, FloatTriple p, float last) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 11*p.x + 13*p.y + 17*p.z + 23*last;
}
double ABI_FN(fp_rollback)(double a, double b, double c, double d, double e,
    double f, double g, double h, IntDouble spill, long w, long x, long y,
    long z, Pair tail) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h +
         11*spill.tag + 13*spill.value + 17*w + 19*x + 23*y + 29*z +
         31*tail.a + 37*tail.b;
}
double ABI_FN(gp_rollback)(long a, long b, long c, long d, long e, long f,
    IntDouble spill, double u, double v, double w, double x, double y, double z,
    DoublePair tail) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*spill.tag + 11*spill.value +
         u + 2*v + 3*w + 4*x + 5*y + 6*z + 13*tail.x + 17*tail.y;
}
double ABI_FN(many_real)(DoublePair a, DoublePair b, DoublePair c,
                         DoublePair d, DoublePair e, long last) {
  return a.x + 2*b.y + 3*c.x + 4*d.y + 5*e.x + last;
}
RealTriple ABI_FN(real_sret)(long a, long b, long c, long d,
                            IntDouble p, DoubleInt q, double last) {
  RealTriple result = {{a + b + c + d + p.tag, p.value + q.value, q.tag + last}};
  return result;
}
double ABI_FN(aligned_sse)(double a, double b, double c, double d, double e,
                           double f, double g, AlignedReal x, float last) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 11*x.value + 17*last;
}
