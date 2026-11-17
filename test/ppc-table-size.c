#include <stdio.h>
#include <stdlib.h>

#ifdef TEST_OPEN
#define FIXED
#else
#define FIXED const
#endif

#if defined(TEST_HUGE) || defined(TEST_OVERFLOW) || defined(TEST_BYTE_OVERFLOW) || defined(TEST_FAIL_ALLOC)
typedef struct Dimension {} enum { one, two, three } FIXED Dimension;
#ifdef TEST_FAIL_ALLOC
// Deterministic fault injection; this program never calls get_spec_ptr, so
// no tag-pointer array needs allocation before the dispatch table.
void *calloc(unsigned long n, unsigned long size) { return NULL; }
void huge<Dimension *a>(void) = 0;
#else
void huge<Dimension *a0, Dimension *a1, Dimension *a2, Dimension *a3,
          Dimension *a4, Dimension *a5, Dimension *a6, Dimension *a7,
          Dimension *a8, Dimension *a9, Dimension *a10, Dimension *a11,
          Dimension *a12, Dimension *a13, Dimension *a14, Dimension *a15
#if defined(TEST_OVERFLOW) || defined(TEST_BYTE_OVERFLOW)
          , Dimension *a16, Dimension *a17, Dimension *a18, Dimension *a19,
          Dimension *a20, Dimension *a21, Dimension *a22, Dimension *a23,
          Dimension *a24, Dimension *a25, Dimension *a26, Dimension *a27,
          Dimension *a28, Dimension *a29, Dimension *a30
#ifdef TEST_OVERFLOW
          , Dimension *a31
#endif
#endif
          >(void) = 0;
#endif
int main(void) { puts("unexpected main"); return 0; }
#else

typedef struct TableLeft {} enum { one } FIXED TableLeft;
typedef struct TableRight {} enum { one, two } FIXED TableRight;
static int left_evaluations, right_evaluations, defaults, specifics;
static TableLeft *left_arg(TableLeft *p) { ++left_evaluations; return p; }
static TableRight *right_arg(TableRight *p) { ++right_evaluations; return p; }
int measure<TableLeft *a, TableRight *b>(int x) { ++defaults; return -x - 3; }
int measure<TableLeft.one *a, TableRight.two *b>(int x) { ++specifics; return x + 17; }
int main(void) {
  int total = 0;
  for (int i = 0; i < get_spec_size(TableLeft); ++i)
    for (int j = 0; j < get_spec_size(TableRight); ++j)
      total += measure<left_arg(get_spec_ptr(TableLeft, i)),
                       right_arg(get_spec_ptr(TableRight, j))>(2);
  if (total != -6 || defaults != 5 || specifics != 1) return 1;
  if (left_evaluations != 6 || right_evaluations != 6) return 2;
  puts("table dimensions OK");
  return 0;
}
#endif
