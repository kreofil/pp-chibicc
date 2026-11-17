#include "api.h"
int puts(const char *s);
int registry_known(void) {
  RegistryLeft a = RegistryLeft.one;
  RegistryRight b = RegistryRight.two;
  return registry_combine<&a, &b>(7);
}
int main(void) {
  if (registry_known() != 507) return 2;
  for (int i = 0; i < 2; i++) {
    RegistryLeft *a = get_spec_ptr(RegistryLeft, i);
    for (int j = 0; j < 3; j++) {
      RegistryRight *b = get_spec_ptr(RegistryRight, j);
      int expected = i == 0 && j == 0 ? 207 : i == 1 && j == 2 ? 507 : 107 + 10 * i + j;
      if (registry_combine<a, b>(7) != expected) return 1;
    }
  }
  puts("closed registry OK");
  return 0;
}
