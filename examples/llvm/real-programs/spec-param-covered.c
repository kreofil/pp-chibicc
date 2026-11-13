// Test adapter: preserve the upstream program and explicitly cover tag 0.
#define main spec_param_original_main
#include "ppc-new-syntax/simplest/spec-param/main.c"
#undef main

void Print<Figure.void *f>() { puts("Figure.void"); }

int main(void) {
  Figure.void zero;
  Print<&zero>();
  return spec_param_original_main();
}
