#include <stdio.h>

typedef struct Sample { int value; } enum { marked } const Sample;

int weight<Sample *p>() { return p->value; }
int weight<Sample.marked *p>() { return 2 * p->value; }

static int evaluate(int n) {
  Sample values[n];
  int result = 0;
  for (int i = 0; i < n; ++i) {
    values[i].value = i + 1;
    if (i % 2) values[i] = Sample.marked;
    result += weight<&values[i]>();
  }
  return result;
}

int main(void) {
  int result = evaluate(5);
  printf("VLA: sum=%d\n", result);
  return result != 21;
}
