#define _DEFAULT_SOURCE 1
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  int *values = reallocarray(NULL, 4, sizeof(int));
  if (!values) return 1;
  for (int i = 0; i < 4; ++i) values[i] = i * 17;

  int *grown = reallocarray(values, 9, sizeof(int));
  if (!grown) { free(values); return 2; }
  values = grown;
  for (int i = 0; i < 4; ++i)
    if (values[i] != i * 17) { free(values); return 3; }

  size_t count = (size_t)-1;
  errno = 0;
  void *overflow = reallocarray(values, count, 2);
  if (overflow) { free(overflow); return 4; }
  if (errno != ENOMEM) { free(values); return 5; }
  // A failed resize must preserve the original allocation and contents.
  for (int i = 0; i < 4; ++i)
    if (values[i] != i * 17) { free(values); return 6; }
  free(values);
  puts("reallocarray OK");
  return 0;
}
