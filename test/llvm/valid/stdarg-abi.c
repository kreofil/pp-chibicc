#define main stdarg_abi_main
#include "../../stdarg-abi.c"
#undef main

int host_checks_varargs(void);
long host_vararg_ints(int n, ...);
double host_vararg_doubles(int n, ...);
int host_passes_list(int n, ...);

int main() {
  if (!host_checks_varargs()) return 2;
  if (host_vararg_ints(12, VA_INTS) != 650) return 3;
  if (host_vararg_doubles(12, VA_DOUBLES) != 650.0) return 4;
  if (!host_passes_list(12, VA_DOUBLES)) return 5;
  return stdarg_abi_main();
}
