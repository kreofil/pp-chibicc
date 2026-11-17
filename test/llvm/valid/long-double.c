#define main long_double_common_main
#include "../../long-double.c"
#undef main
#include <string.h>

int main(void) {
  if (long_double_common_main()) return 1;
  long double zero = ld_identity(0.0L);
  long double negative_zero = -zero;
  long double nan = zero / zero;
  long double infinity = 1.0L / zero;
  CHECK_LD(!zero && !negative_zero && !(_Bool)negative_zero);
  CHECK_LD(1.0L / negative_zero < 0 && infinity > ld_huge);
  CHECK_LD(nan != nan && !(nan == nan));
  CHECK_LD(!(nan < zero) && !(nan <= zero) && !(nan > zero) && !(nan >= zero));
  CHECK_LD(nan && (_Bool)nan && !(!nan));
  CHECK_LD((nan ? 2.5L : 3.5L) == 2.5L && (zero || nan) == 1);
  int effects = 0;
  CHECK_LD((zero && ++effects) == 0 && (nan || ++effects) == 1 && effects == 0);
  CHECK_LD(ld_identity(ld_subnormal) * 2 == 0x1p-16444L);
  union { long double x; unsigned short words[8]; } bits;
  bits.x = ld_identity(ld_negative_zero);
  CHECK_LD(bits.words[0] == 0 && bits.words[4] == 0x8000);
  bits.x = ld_identity(ld_precise);
  CHECK_LD(bits.words[0] == 1 && bits.words[3] == 0x8000 && bits.words[4] == 0x3fff);
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.3Lf %.3Lf", ld_identity(1.25L), -2.5L);
  CHECK_LD(strcmp(buffer, "1.250 -2.500") == 0);
  puts("long double IEEE OK");
  return 0;
}
