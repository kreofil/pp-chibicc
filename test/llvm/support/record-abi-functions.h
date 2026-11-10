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
