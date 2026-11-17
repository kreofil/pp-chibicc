#include <stdio.h>

#define CHECK_LD(expr) do { if (!(expr)) { \
  printf("long double failure at line %d\n", __LINE__); return 1; \
} } while (0)

long double ld_precise = 0x1.0000000000000002p0L;
long double ld_difference = 0x1.0000000000000002p0L - 1.0L;
long double ld_huge = 0x1p12000L;
long double ld_tiny = 0x1p-16000L;
long double ld_subnormal = 0x1p-16445L;
long double ld_from_unsigned = (unsigned long)-1;
long double ld_rounded_double = (double)0x1.0000000000000002p0L;
long double ld_rounded_float = (float)16777217.0L;
unsigned long ld_unsigned_result = (unsigned long)0xf.fffffffffffffffp60L;
unsigned int ld_uint_result = (unsigned int)4294967295.0L;
int ld_truths[] = {0x1.0000000000000002p0L > 1.0L,
                  1.25L == 1.75L, !0.25L, 0.25L && 1, 0.25L || 0,
                  0.25L ? 2 : 3, (int)(0.25L ? 2.0L : 3.0L)};
struct LDStorage { long tag; long double values[2]; };
struct LDStorage ld_storage = {7, {0x1.0000000000000002p0L, -2.5L}};
union LDStorageUnion { long double x; unsigned long bits[2]; };
union LDStorageUnion ld_union = {0x1.0000000000000002p0L};
long double ld_negative_zero = -0.0L;

static long double ld_identity(long double x) { return x; }

int main(void) {
  CHECK_LD(sizeof(long double) == 16 && _Alignof(long double) == 16);
  CHECK_LD(ld_precise > 1.0L && ld_precise - 1.0L == 0x1p-63L);
  CHECK_LD(ld_difference == 0x1p-63L && ld_huge == 0x1p12000L);
  CHECK_LD(ld_tiny == 0x1p-16000L && ld_subnormal == 0x1p-16445L);
  CHECK_LD(ld_from_unsigned == 0xf.fffffffffffffffp60L);
  CHECK_LD(ld_unsigned_result == 18446744073709551615UL);
  CHECK_LD(ld_uint_result == 4294967295U);
  CHECK_LD(ld_rounded_double == 1.0L && ld_rounded_float == 16777216.0L);
  CHECK_LD(ld_truths[0] == 1 && ld_truths[1] == 0 && ld_truths[2] == 0);
  CHECK_LD(ld_truths[3] == 1 && ld_truths[4] == 1 && ld_truths[5] == 2 && ld_truths[6] == 2);
  CHECK_LD(ld_storage.tag == 7 && ld_storage.values[0] == ld_precise);
  CHECK_LD(ld_storage.values[1] == -2.5L && ld_union.x == ld_precise);
  struct LDStorage copy = ld_storage;
  copy.values[0] += 0.5L;
  CHECK_LD(copy.values[0] - ld_storage.values[0] == 0.5L);

  long double x = ld_identity(1.25L);
  CHECK_LD(x + 2.5L == 3.75L && x - 2.5 == -1.25L);
  CHECK_LD(x * 4 == 5.0L && x / 2.0f == 0.625L && -x == -1.25L);
  CHECK_LD((long double)(float)1.25f == x && (long double)(double)2.5 == 2.5L);
  CHECK_LD((double)ld_identity(0x1.0000000000000002p0L) == 1.0);
  CHECK_LD((float)ld_identity(16777217.0L) == 16777216.0f);
  CHECK_LD((long double)18446744073709551615UL == 0xf.fffffffffffffffp60L);
  CHECK_LD((unsigned long)ld_identity(0xf.fffffffffffffffp60L) == 18446744073709551615UL);
  CHECK_LD((unsigned long)ld_identity(0x1p63L) == 9223372036854775808UL);
  CHECK_LD((unsigned long)ld_identity(42.75L) == 42UL);
  CHECK_LD((unsigned int)ld_identity(4294967295.0L) == 4294967295U);
  CHECK_LD((short)ld_identity(-30000.75L) == -30000);
  CHECK_LD((unsigned short)ld_identity(65000.75L) == 65000);
  CHECK_LD((signed char)ld_identity(-120.75L) == -120);
  CHECK_LD((unsigned char)ld_identity(250.75L) == 250);
  CHECK_LD((long)ld_identity(-0x1p63L) == (-9223372036854775807L - 1));
  CHECK_LD((int)ld_identity(-123.75L) == -123);
  CHECK_LD((long double)(_Bool)1 == 1.0L);
  long double (*fp)(long double) = ld_identity;
  CHECK_LD(fp(ld_precise) == ld_precise);
  puts("long double OK");
  return 0;
}
