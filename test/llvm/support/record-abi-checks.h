// CHECK_FN selects functions compiled by the other compiler.
static int check_record_calls(void) {
  One one = {5};
  if (CHECK_FN(one)(one).value != 6 || one.value != 5)
    return 1;
  Three three = {{1, 2, 3}};
  Three three_result = CHECK_FN(three)(three);
  if (three_result.data[0] != 2 || three_result.data[2] != 2 || three.data[0] != 1)
    return 2;
  Twelve twelve = {2, 3, 5};
  if (CHECK_FN(twelve)(twelve).c != 7 || twelve.c != 5)
    return 3;
  Pair pair = {7, 11};
  Pair pair_result = CHECK_FN(pair)(pair);
  if (pair_result.a != 10 || pair_result.b != 9 || pair.b != 11)
    return 4;
  Large large = {5, 6, 7};
  Large large_result = CHECK_FN(large)(large, 4);
  if (large_result.a != 9 || large_result.c != 15 || large.c != 7)
    return 5;
  Packed packed = {'X', 5000000000L};
  Packed packed_result = CHECK_FN(packed)(packed, 7);
  if (packed_result.tag != 'X' || packed_result.value != 5000000007L ||
      packed.value != 5000000000L)
    return 6;
  Aligned aligned = {7};
  if (CHECK_FN(aligned)(aligned, 6).value != 13)
    return 7;
  Nested nested = {1, {2, 3}, 4};
  Nested nested_result = CHECK_FN(nested)(nested);
  if (nested_result.head != 2 || nested_result.inner.y != 5 || nested_result.tail != 7)
    return 8;
  Value value;
  for (int i = 0; i < 12; i++)
    value.bytes[i] = i + 1;
  Value value_result = CHECK_FN(value)(value);
  if (value_result.bytes[0] != 2 || value_result.bytes[11] != 14 || value.bytes[0] != 1)
    return 9;
  if (CHECK_FN(pressure)(1, 2, 3, 4, 5, pair, 13) != 394 ||
      CHECK_FN(boundary)(1, 2, 3, 4, pair, 13) != 369)
    return 10;
  Large pressure = CHECK_FN(sret_pressure)(1, 2, 3, 4, pair, 13);
  if (pressure.a != 10 || pressure.b != 18 || pressure.c != 13)
    return 11;
  if (CHECK_FN(many)(pair, pair, pair, pair, 13) != 49)
    return 12;
  Pair other = {19, 23};
  if (CHECK_FN(select)(0, pair, other).b != 23 ||
      CHECK_FN(compose)(pair, large, 2).a != 14)
    return 13;
  Pair (*pair_callback)(Pair) = CHECK_FN(pair);
  Large (*large_callback)(Large, long) = CHECK_FN(large);
  Packed (*packed_callback)(Packed, long) = CHECK_FN(packed);
  if (pair_callback(pair).a != 10 || large_callback(large, 4).c != 15 ||
      packed_callback(packed, 9).value != 5000000009L)
    return 14;
  int items[3] = {3, 7, 11};
  PointerPair pointer = {items, 2};
  PointerPair pointer_result = CHECK_FN(pointer)(pointer);
  if (pointer_result.pointer != &items[2] || *pointer_result.pointer != 11 ||
      pointer.pointer != items)
    return 15;

  FloatOne fone = {1.25f};
  if (CHECK_FN(float_one)(fone).x != 1.75f || fone.x != 1.25f)
    return 16;
  FloatPair fpair = {1.5f, 2.5f};
  FloatPair fpair_result = CHECK_FN(float_pair)(fpair);
  if (fpair_result.x != 2.0f || fpair_result.y != 2.25f || fpair.y != 2.5f)
    return 17;
  FloatTriple ftriple = {1.5f, 2.5f, 3.5f};
  FloatTriple ftriple_result = CHECK_FN(float_triple)(ftriple);
  if (ftriple_result.x != 1.5f || ftriple_result.y != 2.5f || ftriple_result.z != 5.0f)
    return 18;
  FloatQuad fquad = {{1, 2, 3, 4}};
  FloatQuad fquad_result = CHECK_FN(float_quad)(fquad);
  if (fquad_result.v[0] != 1 || fquad_result.v[2] != 3 || fquad_result.v[3] != 5)
    return 19;
  DoubleOne done = {1.5};
  DoublePair dpair = {7, 11};
  DoublePair dpair_result = CHECK_FN(double_pair)(dpair);
  if (CHECK_FN(double_one)(done).x != 1.75 ||
      dpair_result.x != 18 || dpair_result.y != 10.5 || dpair.x != 7)
    return 20;
  FloatDouble fd = {1.5f, 2.5};
  DoubleFloat df = {2.5, 1.5f};
  FloatDouble fd_result = CHECK_FN(float_double)(fd);
  DoubleFloat df_result = CHECK_FN(double_float)(df);
  if (fd_result.x != 2.0f || fd_result.y != 4.5 ||
      df_result.x != 4.0 || df_result.y != 2.0f)
    return 21;
  IntFloat ifloat = {2, 3.5f};
  FloatInt fint = {3.5f, 2};
  IntFloat ifloat_result = CHECK_FN(int_float)(ifloat);
  FloatInt fint_result = CHECK_FN(float_int)(fint);
  if (ifloat_result.tag != 4 || ifloat_result.value != 4.0f ||
      fint_result.tag != 5 || fint_result.value != 3.0f)
    return 22;
  IntDouble idouble = {7, 11};
  DoubleInt dint = {13, 17};
  IntDouble idouble_result = CHECK_FN(int_double)(idouble);
  DoubleInt dint_result = CHECK_FN(double_int)(dint);
  if (idouble_result.tag != 9 || idouble_result.value != 11.5 ||
      dint_result.tag != 20 || dint_result.value != 12.5)
    return 23;
  NestedReal nested_real = {{{{1, 2}}, {{3, 4}}}};
  NestedReal nested_real_result = CHECK_FN(nested_real)(nested_real);
  if (nested_real_result.parts[0].v[1] != 2 ||
      nested_real_result.parts[1].v[1] != 5 || nested_real.parts[1].v[1] != 4)
    return 24;
  RealUnion real_union = {{1.5f, 2.5f}};
  RealUnion real_union_result = CHECK_FN(real_union)(real_union);
  if (real_union_result.f[0] != 2.0f || real_union_result.f[1] != 2.25f)
    return 25;
  real_union.d = 3.25;
  if (CHECK_FN(real_union_double)(real_union).d != 3.75)
    return 26;
  MixedUnion mixed_union;
  mixed_union.d = 3.25;
  if (CHECK_FN(mixed_union)(mixed_union).d != 5.5)
    return 27;
  WideUnion wide_union = {{1.5f, 2.5f, 3.5f}};
  WideUnion wide_union_result = CHECK_FN(wide_union)(wide_union);
  if (wide_union_result.f[0] != 1.5f || wide_union_result.f[1] != 2.5f ||
      wide_union_result.f[2] != 5.0f)
    return 28;
  PackedReal packed_real = {'P', 2.5};
  PackedReal packed_real_result = CHECK_FN(packed_real)(packed_real, 1.25);
  RealTriple real_triple = {{1.5, 2.5, 3.5}};
  RealTriple real_triple_result = CHECK_FN(real_triple)(real_triple, 0.5);
  if (packed_real_result.tag != 'P' || packed_real_result.value != 3.75 ||
      packed_real.value != 2.5 || real_triple_result.v[0] != 2.0 ||
      real_triple_result.v[2] != 4.5 || real_triple.v[0] != 1.5)
    return 29;
  AlignedReal aligned_real = {7.25f};
  if (CHECK_FN(aligned_real)(aligned_real, 0.5f).value != 7.75f ||
      CHECK_FN(aligned_sse)(1, 2, 3, 4, 5, 6, 7, aligned_real, 13) != 440.75)
    return 30;
  if (CHECK_FN(sse_pressure)(1, 2, 3, 4, 5, 6, 7, dpair, 13) != 581 ||
      CHECK_FN(sse_boundary)(1, 2, 3, 4, 5, 6, dpair, 13) != 532)
    return 31;
  FloatPair vector_pair = {7, 11};
  FloatTriple vector_triple = {7, 11, 13};
  if (CHECK_FN(vector_boundary)(1, 2, 3, 4, 5, 6, 7, vector_pair, 13) != 581 ||
      CHECK_FN(vector_pressure)(1, 2, 3, 4, 5, 6, 7, vector_triple, 19) != 1018)
    return 32;
  if (CHECK_FN(fp_rollback)(1, 2, 3, 4, 5, 6, 7, 8, idouble, 1, 2, 3, 4, pair) != 1288 ||
      CHECK_FN(gp_rollback)(1, 2, 3, 4, 5, 6, idouble, 1, 2, 3, 4, 5, 6, dpair) != 630)
    return 33;
  if (CHECK_FN(many_real)(dpair, dpair, dpair, dpair, dpair, 13) != 142)
    return 34;
  RealTriple real_pressure = CHECK_FN(real_sret)(1, 2, 3, 4, idouble, dint, 19);
  if (real_pressure.v[0] != 17 || real_pressure.v[1] != 24 || real_pressure.v[2] != 36)
    return 35;
  FloatTriple (*float_callback)(FloatTriple) = CHECK_FN(float_triple);
  DoubleInt (*mixed_callback)(DoubleInt) = CHECK_FN(double_int);
  RealTriple (*real_callback)(RealTriple, double) = CHECK_FN(real_triple);
  if (float_callback(ftriple).z != 5.0f || mixed_callback(dint).value != 12.5 ||
      real_callback(real_triple, 0.5).v[2] != 4.5)
    return 36;
  return 0;
}
