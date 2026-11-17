#include <stdio.h>

// Out-of-range narrow labels deliberately check integer promotions. Some
// host compilers warn about those valid but unreachable labels and case_in_if.
#define CHECK(want, expression) do { \
  int actual = (expression); \
  if (actual != (want)) { \
    fprintf(stderr, "switch check line %d: got %d, expected %d\n", __LINE__, actual, (want)); \
    return 1; \
  } \
} while (0)

#define LONG_MIN_VALUE (-9223372036854775807L - 1)
#define LONG_MAX_VALUE 9223372036854775807L
#define ULONG_MAX_VALUE 0xffffffffffffffffUL

static long folded_signed = (int)0xffffffffU;
static unsigned long folded_unsigned = ~0U;
static unsigned long folded_wrap = (0xffffffffU + 1U) / 2U;
static int folded_comparison = (int)0xffffffffU < 0;

static int fallthrough(int value) {
  int result = 1;
  switch (value) {
  case -3: return 7;
  case 0: result += 2; break;
  case 1: result += 10;
  default: result += 20;
  case 2: result += 30; break;
  case 3: case 4: case 5: result = 8;
  }
  return result;
}

static int no_default(int value) {
  int result = 5;
  switch (value) { case 2: result = 9; }
  return result;
}

static int default_only(int value) {
  switch (value) default: return 4;
}

static int narrow_signed(signed char value) {
  switch (value) { case -1: return 1; case 127: return 2; case 255: return 3; }
  return 4;
}

static int narrow_unsigned(unsigned char value) {
  switch (value) { case -1: return 1; case 255: return 2; case 256: return 3; }
  return 4;
}

static int narrow_short(unsigned short value) {
  switch (value) { case -1: return 1; case 65535: return 2; case 65536: return 3; }
  return 4;
}

static int boolean(_Bool value) {
  switch (value) { case 0: return 2; case 1: return 3; case 2: return 5; }
  return 4;
}

enum Choice { negative = -2, positive = 4 };
static int enumeration(enum Choice value) {
  switch (value) { case negative: return 1; case positive: return 2; }
  return 3;
}

static int unsigned32(unsigned int value) {
  switch (value) {
  case 0: return 1;
  case 0x80000000U: return 2;
  case -1: return 3;
  }
  return 4;
}

static int signed64(long value) {
  switch (value) {
  case -1: return 1;
  case 0xffffffffL: return 2;
  case 0x100000000L: return 3;
  case LONG_MIN_VALUE: return 4;
  case LONG_MAX_VALUE: return 5;
  case (0xffffffffU + 1U): return 7;
  }
  return 6;
}

static int unsigned64(unsigned long value) {
  switch (value) {
  case 0: return 1;
  case 0x100000000UL: return 2;
  case 0x8000000000000000UL: return 3;
  case ULONG_MAX_VALUE: return 4;
  }
  return 5;
}

static int nested(void) {
  int result = 0;
  for (int i = 0; i < 6; i++) {
    switch (i % 3) {
    case 0:
      for (int j = 0; j < 4; j++) {
        if (j == 2) break;
        result++;
      }
      break;
    case 1: result += 10; continue;
    case 2:
      switch (i) { case 2: result += 20; break; default: result += 30; }
    }
    result += 100;
  }
  return result;
}

static int duff(int count) {
  int result = 0;
  if (!count) return 0;
  int rounds = (count + 3) / 4;
  switch (count % 4) {
  case 0: do { result++;
  case 3: result++;
  case 2: result++;
  case 1: result++;
  } while (--rounds > 0);
  }
  return result;
}

static int case_in_if(int value) {
  int result = 0;
  switch (value) {
    if (0) { case 1: result = 9; break; }
  default: result = 4;
  }
  return result;
}

static int goto_label(void) {
  int result = 0;
  goto inside;
  switch (0) { case 1: inside: result = 7; break; default: result = 3; }
  return result;
}

static int range_signed(int value) {
  switch (value) { case -4 ... 2: return 1; case 8: return 2; default: return 3; }
}

static int range_converted(int value) {
  switch (value) { case 0xfffffffeU ... 0xffffffffU: return 1; default: return 2; }
}

static int range_unsigned32(unsigned int value) {
  switch (value) {
  case 0 ... 3: return 1;
  case 0xfffffff0U ... 0xffffffffU: return 2;
  default: return 3;
  }
}

static int range_signed64(long value) {
  switch (value) {
  case LONG_MIN_VALUE ... -1: return 1;
  case 0 ... LONG_MAX_VALUE: return 2;
  }
  return 3;
}

static int range_full(unsigned long value) {
  switch (value) { case 0 ... ULONG_MAX_VALUE: return 7; }
  return 8;
}

int main(void) {
  CHECK(1, folded_signed == -1);
  CHECK(1, folded_unsigned == 0xffffffffUL);
  CHECK(1, folded_wrap == 0);
  CHECK(1, folded_comparison);
  CHECK(7, fallthrough(-3));
  CHECK(3, fallthrough(0));
  CHECK(61, fallthrough(1));
  CHECK(31, fallthrough(2));
  CHECK(8, fallthrough(3));
  CHECK(8, fallthrough(4));
  CHECK(8, fallthrough(5));
  CHECK(51, fallthrough(99));
  CHECK(5, no_default(0));
  CHECK(9, no_default(2));
  CHECK(4, default_only(12));
  CHECK(1, narrow_signed(-1));
  CHECK(2, narrow_signed(127));
  CHECK(2, narrow_unsigned(255));
  CHECK(4, narrow_unsigned(0));
  CHECK(2, narrow_short(65535));
  CHECK(2, boolean(0));
  CHECK(3, boolean(1));
  CHECK(1, enumeration(negative));
  CHECK(2, enumeration(positive));
  CHECK(3, enumeration(0));
  CHECK(1, unsigned32(0));
  CHECK(2, unsigned32(0x80000000U));
  CHECK(3, unsigned32(0xffffffffU));
  CHECK(4, unsigned32(1));
  CHECK(1, signed64(-1));
  CHECK(2, signed64(0xffffffffL));
  CHECK(3, signed64(0x100000000L));
  CHECK(4, signed64(LONG_MIN_VALUE));
  CHECK(5, signed64(LONG_MAX_VALUE));
  CHECK(7, signed64(0));
  CHECK(6, signed64(1));
  CHECK(1, unsigned64(0));
  CHECK(2, unsigned64(0x100000000UL));
  CHECK(3, unsigned64(0x8000000000000000UL));
  CHECK(4, unsigned64(ULONG_MAX_VALUE));
  CHECK(5, unsigned64(1));
  CHECK(474, nested());
  for (int i = 0; i <= 33; i++) CHECK(i, duff(i));
  CHECK(9, case_in_if(1));
  CHECK(4, case_in_if(2));
  CHECK(7, goto_label());
  for (int i = -6; i <= 10; i++)
    CHECK(i >= -4 && i <= 2 ? 1 : i == 8 ? 2 : 3, range_signed(i));
  CHECK(1, range_converted(-2));
  CHECK(1, range_converted(-1));
  CHECK(2, range_converted(0));
  CHECK(1, range_unsigned32(0));
  CHECK(1, range_unsigned32(3));
  CHECK(2, range_unsigned32(0xfffffff0U));
  CHECK(2, range_unsigned32(0xffffffffU));
  CHECK(3, range_unsigned32(0xffffffefU));
  CHECK(1, range_signed64(LONG_MIN_VALUE));
  CHECK(1, range_signed64(-1));
  CHECK(2, range_signed64(0));
  CHECK(2, range_signed64(LONG_MAX_VALUE));
  CHECK(7, range_full(0));
  CHECK(7, range_full(0x8000000000000000UL));
  CHECK(7, range_full(ULONG_MAX_VALUE));

  int evaluations = 0;
  switch (evaluations++) { }
  CHECK(1, evaluations);
  switch (++evaluations) { default: break; }
  CHECK(2, evaluations);
  switch (evaluations++) { case 2: break; default: return 1; }
  CHECK(3, evaluations);
  puts("switch OK");
  return 0;
}
