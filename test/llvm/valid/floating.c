#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
  printf("floating failure at line %d\n", __LINE__); return 1; \
} } while (0)

float global_float = 1.25f;
double global_double = -2.5;
float global_array[] = {0.5f, 1.5f, 2.5f};
struct Values { float f; double d; float array[2]; };
struct Values global_values = {1.5f, 2.25, {3.5f, 4.5f}};

static float identity_float(float x) { return x; }
static double identity_double(double x) { return x; }
static double sum_values(struct Values *v) {
  return v->f + v->d + v->array[0] + v->array[1];
}

int main(void) {
  CHECK(global_float == 1.25f && global_double == -2.5);
  CHECK(global_array[0] + global_array[1] + global_array[2] == 4.5f);
  struct Values local = global_values;
  local.f += 0.5f;
  local.d *= 2.0;
  local.array[1] = 5.0f;
  CHECK(sum_values(&local) == 15.0 && sum_values(&global_values) == 11.75);

  float f = identity_float(1.25f);
  double d = identity_double(2.5);
  CHECK(f + d == 3.75 && d - f == 1.25);
  CHECK(f * d == 3.125 && d / f == 2.0);
  CHECK(-f == -1.25f && -d == -2.5);
  CHECK(f < d && f <= d && d > f && d >= f && f != d);
  CHECK(f == 1.25f && f <= 1.25f && f >= 1.25f);
  CHECK(!(f > d) && !(d < f) && !(f == d) && !(f != 1.25f));
  CHECK((1 ? f : d) == 1.25 && (0 ? d : f) == 1.25);

  int iterations = 0;
  for (float remaining = 3.0f; remaining; remaining--)
    iterations++;
  double remaining = 2.0;
  while (remaining) { remaining--; iterations++; }
  CHECK(iterations == 5);
  CHECK(f++ == 1.25f && f == 2.25f && --f == 1.25f);
  f *= 4.0f;
  f /= 2.0f;
  f -= 0.5f;
  CHECK(f == 2.0f);

  CHECK((double)identity_float(0.1f) == 0.100000001490116119384765625);
  CHECK((float)identity_double(16777217.0) == 16777216.0f);
  CHECK(identity_float(0x1p-149f) == 0x1p-149f);
  CHECK(identity_double(0x1p-1074) == 0x1p-1074);
  union { float f; unsigned int bits; } float_bits = {1.23456789f};
  union { double d; unsigned long bits; } double_bits = {1.2345678901234567};
  CHECK(float_bits.bits == 0x3f9e0652U);
  CHECK(double_bits.bits == 0x3ff3c0ca428c59fbUL);

  int signed_int = -1234567;
  unsigned int unsigned_int = 4000000000U;
  long signed_long = -9007199254740992L;
  unsigned long unsigned_long = 18446744073709549568UL;
  CHECK((double)signed_int == -1234567.0);
  CHECK((double)unsigned_int == 4000000000.0);
  CHECK((double)signed_long == -0x1p53);
  CHECK((double)unsigned_long == 0x1.fffffffffffffp63);
  CHECK((float)signed_int == -1234567.0f && (float)unsigned_int == 4000000000.0f);
  CHECK((float)signed_long == -0x1p53f && (float)unsigned_long == 0x1p64f);
  CHECK((double)(_Bool)7 == 1.0 && (float)(_Bool)0 == 0.0f);
  CHECK((int)identity_double(-123.75) == -123);
  CHECK((unsigned int)identity_double(4294967295.0) == 4294967295U);
  CHECK((long)identity_double(-0x1p63) == (-9223372036854775807L - 1));
  CHECK((unsigned long)identity_double(0x1.fffffffffffffp63) == unsigned_long);
  CHECK((signed char)identity_float(-120.75f) == -120);
  CHECK((unsigned char)identity_double(250.75) == 250);
  CHECK((short)identity_double(-30000.75) == -30000);
  CHECK((unsigned short)identity_float(65000.5f) == 65000);

  // Exercise runtime operations: ordered comparisons are false for NaNs,
  // but !=, conditions and conversion to _Bool must treat a NaN as true.
  double zero = identity_double(0.0);
  double negative_zero = -zero;
  double nan = zero / zero;
  double infinity = 1.0 / zero;
  CHECK(!zero && !negative_zero && !(_Bool)negative_zero);
  CHECK(1.0 / negative_zero < 0.0 && infinity > 0.0);
  CHECK(nan != nan && !(nan == nan));
  CHECK(!(nan < zero) && !(nan <= zero) && !(nan > zero) && !(nan >= zero));
  CHECK(nan && (_Bool)nan && !(!nan));
  CHECK((nan && 3) == 1 && (zero || nan) == 1);
  CHECK((nan ? 2.5 : 3.5) == 2.5);
  int side_effect = 0;
  CHECK((zero && ++side_effect) == 0 && (nan || ++side_effect) == 1);
  CHECK(side_effect == 0);
  float float_zero = identity_float(0.0f);
  float float_nan = float_zero / float_zero;
  CHECK(float_nan != float_nan && !(float_nan == float_nan));
  CHECK(!(float_nan < float_zero) && !(float_nan <= float_zero));
  CHECK(float_nan && (_Bool)float_nan && !(-float_zero));
  CHECK(1.0f / -float_zero < 0.0f);

  printf("floating OK: %.2f %.2f\n", f, d);
  return 0;
}
