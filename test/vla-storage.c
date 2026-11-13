#include <stdio.h>
// Some libc headers hide GNU attributes from compilers without __GNUC__.
#undef __attribute__
#ifndef __chibicc__
#include <alloca.h>
#endif

#define CHECK(expr) do { if (!(expr)) { printf("VLA failure: %d\n", __LINE__); return 1; } } while (0)

static int bound_calls;
static int bound(int n) { ++bound_calls; return n; }

static long sum(int *values, int n) {
  long result = 0;
  for (int i = 0; i < n; ++i) result += values[i];
  return result;
}

static char *nested(int a, char *p, int b, int c, int d, int e, int f, int g, int h) {
  if (a + b + c + d + e + f + g + h != 37) return 0;
  return p;
}

static int recursive(int n) {
  if (!n) return 0;
  int values[n];
  for (int i = 0; i < n; ++i) values[i] = n * 10 + i;
  int result = recursive(n - 1);
  for (int i = 0; i < n; ++i)
    if (values[i] != n * 10 + i) return -10000;
  return result + values[n - 1];
}

struct __attribute__((aligned(64))) WideElement { long value; };

int main(void) {
  // The built-in accepts size_t rather than truncating an explicit call to int.
  CHECK(_Generic(&alloca, void *(*)(unsigned long): 1, default: 0));
  int rows = 3, cols = 5;
  bound_calls = 0;
  int matrix[bound(rows)][bound(cols)];
  CHECK(bound_calls == 2);
  CHECK(sizeof(matrix) == 60 && sizeof(matrix[0]) == 20);
  CHECK(_Alignof(int[rows]) == _Alignof(int));
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) matrix[r][c] = r * 100 + c;
  CHECK(sum(matrix[0], cols) == 10 && sum(matrix[2], cols) == 1010);
  CHECK((char *)&matrix == (char *)&matrix[0][0]);
  CHECK(&matrix[2] - &matrix[0] == 2);
  CHECK(&matrix[0] - &matrix[2] == -2 && &matrix[1] - &matrix[1] == 0);
  CHECK((matrix + rows - 2)[0][4] == 104);
  CHECK((char *)(matrix + rows) - (char *)matrix == 60);
  int (*alias)[cols] = matrix;
  cols = 9;
  CHECK(sizeof(*alias) == 20 && alias[2][4] == 204);
  CHECK(bound_calls == 2);

  int n = 7;
  bound_calls = 0;
  typedef int Row[bound(n)];
  CHECK(bound_calls == 1 && sizeof(Row) == 28);
  n = 9;
  Row table[2];
  CHECK(sizeof(table) == 56 && sizeof(Row) == 28);
  CHECK(bound_calls == 1);
  {
    // A previously captured VLA typedef decays to a pointer in a prototype.
    long sum(Row values, int count);
    for (int i = 0; i < 7; ++i) table[1][i] = i;
    CHECK(sum(table[1], 7) == 21);
  }
  table[1][6] = 77;
  CHECK(table[1][6] == 77);
  bound_calls = 0;
  CHECK(sizeof(int[bound(7)]) == 28 && bound_calls == 1);
  bound_calls = 0;
  for (int size = 1; size <= 4; ++size) {
    typedef int Iteration[bound(size)];
    Iteration values;
    values[size - 1] = size;
    CHECK(sizeof(values) == size * sizeof(int) && values[size - 1] == size);
  }
  CHECK(bound_calls == 4);
  // sizeof a VLA type computes size_t without allocating that storage.
  int large_bound = 1073741824;
  CHECK(sizeof(int[large_bound]) == 4294967296UL);
  int (*large_row)[large_bound] = 0;
  CHECK(sizeof(*large_row) == 4294967296UL);

  int depth = 2, height = 3, width = 4;
  int cube[depth][height][width];
  cube[1][2][3] = 123;
  CHECK(sizeof(cube) == 96 && sizeof(cube[0]) == 48);
  CHECK(sizeof(cube[0][0]) == 16 && cube[1][2][3] == 123);
  CHECK(recursive(7) == 301);

  // Alignment must apply to the payload, independently of the pointer slot.
  for (int count = 1; count <= 7; ++count) {
    struct WideElement wide[count];
    _Alignas(64) char bytes[count + 3];
    long double extended[count];
    CHECK(_Alignof(struct WideElement[count]) == 64);
    CHECK((unsigned long)wide % 64 == 0 && (unsigned long)bytes % 64 == 0);
    CHECK((unsigned long)extended % 16 == 0);
    wide[count - 1].value = count * 19;
    bytes[count + 2] = count;
    extended[count - 1] = 1.25L;
    CHECK(wide[count - 1].value == count * 19 && bytes[count + 2] == count);
    CHECK(extended[count - 1] == 1.25L);
  }

  bound_calls = 0;
  char *first = alloca(bound(17));
  char *second = nested(1, alloca(bound(19)), 3, 4, 5, 6, 7, 8, 3);
  CHECK(second && bound_calls == 2);
  CHECK((unsigned long)first % 16 == 0 && (unsigned long)second % 16 == 0);
  for (int i = 0; i < 17; ++i) first[i] = i + 1;
  for (int i = 0; i < 19; ++i) second[i] = i + 21;

  // Explicit alloca storage lasts until return, including allocations in loops.
  unsigned char *saved[12];
  for (int i = 0; i < 12; ++i) {
    saved[i] = alloca(i + 1);
    for (int j = 0; j <= i; ++j) saved[i][j] = i * 13 + j;
  }
  for (int i = 0; i < 12; ++i)
    for (int j = 0; j <= i; ++j) CHECK(saved[i][j] == i * 13 + j);
  for (int i = 0; i < 17; ++i) CHECK(first[i] == i + 1);
  for (int i = 0; i < 19; ++i) CHECK(second[i] == i + 21);
  puts("VLA storage OK");
  return 0;
}
