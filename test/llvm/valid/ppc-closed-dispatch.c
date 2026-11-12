#define DISPATCH_EXTERNAL_HELPER 1
#define main closed_dispatch_main
#include "../../ppc-closed-dispatch.c"
#undef main
int host_active_second(void);
int main(int argc, char **argv) {
  dispatch_second_base = host_active_second();
  if (dispatch_second_base != (argc > 1 ? 30 : 300)) return 4;
  return closed_dispatch_main();
}
