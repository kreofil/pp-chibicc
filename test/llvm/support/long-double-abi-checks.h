#define LD_CAT_I(a, b) a##_##b
#define LD_CAT(a, b) LD_CAT_I(a, b)
#define LD_FN(name) LD_CAT(LD_CALLEE, name)
#define LD_CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)
static int LD_CAT(LD_CHECKER, checks)(void) {
  LD_CHECK(LD_FN(scalar)(0x1.0000000000000002p0L, 0, 1.0) > 1.0L);
  long double (*scalar)(long double, int, double) = LD_FN(scalar);
  LD_CHECK(scalar(1.25L, 3, 2.0) == 5.5L);
  LDFpPair fp = {8, 9};
  LD_CHECK(LD_FN(sse_budget)(1.0L, 2, 3, 4, 5, 6, 7, fp, 10) == 55.0L);
  LDIntPair ip = {6, 7};
  LD_CHECK(LD_FN(gp_budget)(1.0L, 2, 3, 4, 5, ip, 8) == 36.0L);
  LDBox box = {1.25L};
  LD_CHECK(LD_FN(box_budget)(box, 2, 3, 4, 5, 6, 7, fp) == 45.25L);
  LDBox (*box_fn)(LDBox) = LD_FN(box);
  LDBox next = box_fn(box);
  LD_CHECK(next.x == 1.75L && box.x == 1.25L);
  LDNested nested = {{{2.5L}}};
  LD_CHECK(LD_FN(nested)(nested).inner.items[0] == 5.0L);
  LDX87 x87 = {1.25L};
  LD_CHECK(LD_FN(x87)(x87).x == 2.0L);
  LDMixed mixed = {1.25L};
  LDMixed (*mixed_fn)(LDMixed) = LD_FN(mixed);
  LD_CHECK(mixed_fn(mixed).x == 2.5L);
  LDFloatUnion floating = {1.25L};
  LD_CHECK(LD_FN(float_union)(floating).x == 3.75L);
  LDIntegerUnion integers;
  integers.words[0] = 1;
  integers.words[1] = 2;
  LDIntegerUnion changed = LD_FN(integer_union)(integers);
  LD_CHECK(changed.words[0] == 4 && changed.words[1] == 6);
  LDLarge large = {{1.25L, 2.5L}, 7};
  LDLarge (*large_fn)(LDLarge) = LD_FN(large);
  LDLarge larger = large_fn(large);
  LD_CHECK(larger.items[0] == 2.25L && larger.items[1] == 5.0L && larger.tag == 8);
  LDPacked packed = {3, 1.25L};
  LD_CHECK(LD_FN(packed)(packed).x == 4.25L && LD_FN(packed)(packed).tag == 4);
  LD_CHECK(LD_FN(variadic)(1.0L, 10, LD_TRIPLES) == 2393.5L);
  LD_CHECK(LD_FN(forward)(10, LD_TRIPLES) == 1196.25L);
  LD_CHECK(LD_FN(variadic_box)(2, 1.25L, 2.5L).x == 3.75L);
  LDLarge from_args = LD_FN(variadic_large)(3, 1.25L, 2.5L, 3.0L);
  LD_CHECK(from_args.items[0] == 4.25L && from_args.items[1] == 2.5L && from_args.tag == 3);
  return 0;
}
#undef LD_CHECK
#undef LD_FN
#undef LD_CAT
#undef LD_CAT_I
#undef LD_CALLEE
#undef LD_CHECKER
