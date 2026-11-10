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
  return 0;
}
