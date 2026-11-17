#define MULTIMETHOD_EXTERNAL 1
#define main closed_multimethod_main
#include "../../ppc-closed-multimethod.c"
#undef main
int main(void) {
  multi_override_base = 900;
  return closed_multimethod_main();
}
