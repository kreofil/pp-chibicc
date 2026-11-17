// Exercise the repository example's own tag-0 handler before its main.
#define main spec_param_original_main
#include "spec-param-current-main.c"
#undef main
int main(void) {
  Figure.void zero;
  Print<&zero>();
  return spec_param_original_main();
}
