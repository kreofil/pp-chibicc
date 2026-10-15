#include "test.h"

typedef struct CountFigure { int common; } case { int rect; } CountFigure;

int CountKind<CountFigure *f>() { return 0; }
int CountKind<CountFigure.rect *r>() { return 1; }

static int count_calls;

static int runtime_count(int n) {
  count_calls++;
  return n;
}

int main() {
  int n = 3;

  count_calls = 0;
  CountFigure.rect *created = create_spec(CountFigure.rect, runtime_count(n));
  ASSERT(1, count_calls);
  ASSERT(1, CountKind<&created[0]>());
  ASSERT(1, CountKind<&created[1]>());
  ASSERT(1, CountKind<&created[2]>());

  count_calls = 0;
  union {
    CountFigure.rect item[3];
    char buf[sizeof(CountFigure.rect) * 3];
  } storage;
  memset(storage.buf, 0, sizeof(storage.buf));
  init_spec(CountFigure.rect, (void *)storage.buf, runtime_count(n));
  ASSERT(1, count_calls);
  ASSERT(1, CountKind<&storage.item[0]>());
  ASSERT(1, CountKind<&storage.item[1]>());
  ASSERT(1, CountKind<&storage.item[2]>());

  CountFigure.rect *single = create_spec(CountFigure.rect);
  ASSERT(1, CountKind<single>());

  CountFigure.rect one;
  *(int *)&one = 0;
  init_spec(CountFigure.rect, &one);
  ASSERT(1, CountKind<&one>());

  count_calls = 0;
  CountFigure.rect *empty = create_spec(CountFigure.rect, runtime_count(0));
  ASSERT(1, count_calls);
  ASSERT(1, empty == 0);

  count_calls = 0;
  init_spec(CountFigure.rect, (void *)0, runtime_count(0));
  ASSERT(1, count_calls);

  printf("OK\n");
  return 0;
}
