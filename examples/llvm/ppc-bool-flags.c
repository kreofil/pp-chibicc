#include <stdio.h>

typedef struct Policy { bool enabled; } enum { inverted } const Policy;

bool allowed<Policy *policy>(bool requested) {
  return policy->enabled && requested;
}

bool allowed<Policy.inverted *policy>(bool requested) {
  return policy->enabled && !requested;
}

int main(void) {
  Policy regular = {true};
  Policy.inverted inverted = {true};
  bool first = allowed<&regular>(true);
  bool second = allowed<&inverted>(true);
  printf("regular=%d, inverted=%d\n", first, second);
  return !first || second;
}
