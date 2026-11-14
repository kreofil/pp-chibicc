#include <stdio.h>
#include <stdlib.h>

// Reuse the native assertion-based sources without adding their verbose
// success trace to the LLVM fixtures. A failed assertion still fails the run.
void assert(int expected, int actual, char *code) {
  if (expected != actual) {
    fprintf(stderr, "%s: expected %d, got %d\n", code, expected, actual);
    exit(1);
  }
}
