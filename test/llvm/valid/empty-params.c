// Keep native/stage2 coverage and add independently compiled host ABI calls.
#define main empty_params_main
#include "../../empty-params.c"
#undef main

int host_checks_empty();
double host_without_prototype();
int main() {
  if (!host_checks_empty()) return 2;
  float f = 1.5f;
  signed char c = -2;
  unsigned char u = 250;
  if (host_without_prototype(f, c, u) != 249.5) return 3;
  double (*pointer)() = host_without_prototype;
  if (pointer(f, c, u) != 249.5) return 4;
  return empty_params_main();
}
