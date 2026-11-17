#define NAME_(prefix, suffix) prefix##suffix
#define NAME(prefix, suffix) NAME_(prefix, suffix)

int NAME(FP_CHECK, _checks)(void) {
  int value = 6;
  IntegerPair pair = {10, 20};
  if (NAME(FP_CALLEE, _float)(1.25f, 2) != 3.25f)
    return 0;
  if (NAME(FP_CALLEE, _mixed)(1, 2.0, 3.0f, 4, 5.0, &value) != 91.0)
    return 0;
  if (NAME(FP_CALLEE, _stack)(1.0f, 2.0, 3.0f, 4.0, 5.0f,
                            6.0, 7.0f, 8.0, 9.0f, 10.0) != 385.0)
    return 0;
  // Four GP scalars plus an INTEGER pair fit exactly in six GP registers.
  // The interleaved floating-point arguments must not consume GP registers.
  IntegerPair result = NAME(FP_CALLEE, _pair)(1.5, 2, 3.5f, 4, 5, 6, pair);
  if (result.a != 17 || result.b != 34 || pair.a != 10 || pair.b != 20)
    return 0;
  if (NAME(FP_CALLEE, _gp)(1, 2, 3, 4, 5, 6, pair) != 141)
    return 0;
  float (*float_callback)(float, int) = NAME(FP_CALLEE, _float);
  double (*double_callback)(long, double, float, int, double, const int *) =
    NAME(FP_CALLEE, _mixed);
  return float_callback(-2.5f, 1) == -1.5f &&
         double_callback(1, 2.0, 3.0f, 4, 5.0, &value) == 91.0;
}

#undef NAME
#undef NAME_
