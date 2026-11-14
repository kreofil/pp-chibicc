#include <stdio.h>
#include <stdarg.h>

#define CHECK(expr) do { if (!(expr)) { printf("VLA parameter failure: %d\n", __LINE__); return 1; } } while (0)

static int calls;
static inline int capture(int n) { ++calls; return n; }

int vla_unused_application(int);
static inline int prototype_dependency(int n) { return vla_unused_application(n); }
int unused_dependency(int n, int a[prototype_dependency(n)]);
typedef int Dimension;
int shadowed_typedef(int Dimension, int a[Dimension]);
Dimension file_dimension;

// Prototype bounds and their names have no runtime effects or file scope.
int prototype_only(int n, int a[++calls][n]);
long vla_param_sum(int height, int width, int matrix[height][width]);
long vla_param_sum(int, int, int [*][*]);

long vla_param_sum(int rows, int cols, int matrix[rows][cols]) {
  int width = cols;
  cols = 1;
  if (sizeof(matrix) != sizeof(int *) || sizeof(*matrix) != width * sizeof(int)) return -1000;
  if (matrix - (matrix + rows) != -rows) return -2000;
  long result = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < width; ++c) result += matrix[r][c];
  return result;
}

typedef struct VLAParamReport { long value, row_bytes, pointer_bytes; } VLAParamReport;

static inline VLAParamReport dimensions(int n) {
  ++calls;
  return (VLAParamReport){n, n * 2, n * 3};
}

static VLAParamReport measured(int rows, int cols,
                               int matrix[capture(rows)][capture(cols)]) {
  return (VLAParamReport){vla_param_sum(rows, cols, matrix), sizeof(*matrix), sizeof(matrix)};
}

static long by_pointer(int rows, int cols, int (*matrix)[cols]) {
  return vla_param_sum(rows, cols, matrix);
}

static long by_record(VLAParamReport shape, int matrix[shape.value][shape.row_bytes]) {
  return vla_param_sum(shape.value, shape.row_bytes, matrix);
}

// A bound call returning a record needs its own hidden result buffer.
static long by_call(int rows, int cols, int matrix[rows][dimensions(cols).value]) {
  return vla_param_sum(rows, cols, matrix);
}

static long by_qualified(int rows, int cols, int matrix[static const restrict rows][cols]) {
  return vla_param_sum(rows, cols, matrix);
}

static long invoke(long callback(int n, int m, int values[n][m]),
                   int rows, int cols, int matrix[rows][cols]) {
  return callback(rows, cols, matrix);
}

static long extra(int rows, int cols, int matrix[rows][cols], int count, ...) {
  va_list ap;
  va_start(ap, count);
  long result = vla_param_sum(rows, cols, matrix);
  for (int i = 0; i < count; ++i) result += va_arg(ap, long);
  va_end(ap);
  return result;
}

static int cube(int depth, int rows, int cols, int values[depth][rows][cols]) {
  int width = cols;
  cols = 1;
  return sizeof(*values) == rows * width * sizeof(int) &&
         sizeof(**values) == width * sizeof(int) && values[depth - 1][rows - 1][width - 1] == 37;
}

static unsigned long huge_row(int count, int (*values)[count]) { return sizeof(*values); }

static int dependent(int width, int values[1][width], char bytes[capture(sizeof(*values))]) {
  return sizeof(*values) == width * sizeof(int) && calls == 1;
}

// The prototype's callback names must not hide the containing function's n.
static long nested_scope(int n, long (*callback)(int n, int m, int a[n][m]),
                        int matrix[1][n]) { return callback(1, n, matrix); }

static int callback_scope(int width, int (*callback)(int a[capture(width)])) {
  return width;
}

static int bound_expression(int n, char bytes[capture(sizeof(int[n]))]) {
  return n;
}

#ifdef TEST_VLA_PARAMS_HOST
long host_vla_param_sum(int, int, int (*)[*]);
int host_vla_param_check(void);
#endif

int main(void) {
  CHECK(calls == 0);
  CHECK(file_dimension == 0 && sizeof(Dimension) == sizeof(int));
  int rows = 3, cols = 4;
  int matrix[rows][cols];
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) matrix[r][c] = r * 10 + c + 1;
  CHECK(vla_param_sum(rows, cols, matrix) == 150);
  CHECK(by_pointer(rows, cols, matrix) == 150);
  CHECK(by_record((VLAParamReport){rows, cols, 0}, matrix) == 150);
  CHECK(by_qualified(rows, cols, matrix) == 150);
  CHECK(invoke(vla_param_sum, rows, cols, matrix) == 150);
  CHECK(extra(rows, cols, matrix, 3, 5L, 7L, 11L) == 173);
  calls = 0;
  VLAParamReport report = measured(rows, cols, matrix);
  CHECK(calls == 2 && report.value == 150 && report.row_bytes == 16 && report.pointer_bytes == 8);
  calls = 0;
  CHECK(by_call(rows, cols, matrix) == 150 && calls == 1);
  calls = 0;
  char bytes[16];
  CHECK(dependent(cols, matrix, bytes));
  calls = 0;
  CHECK(callback_scope(cols, 0) == cols && calls == 0);
  CHECK(bound_expression(cols, bytes) == cols && calls == 1);
  CHECK(nested_scope(cols, vla_param_sum, matrix) == 10);
  int depth = 2;
  int values[depth][rows][cols];
  values[1][2][3] = 37;
  CHECK(cube(depth, rows, cols, values));
  CHECK(huge_row(1073741824, 0) == 4294967296UL);
  typedef long (*MatrixFn)(int r, int c, int a[r][c]);
  MatrixFn selected = vla_param_sum;
  CHECK(selected(rows, cols, matrix) == 150);
  CHECK(_Generic(selected, long (*)(int, int, int (*)[4]): 1, default: 0));
  CHECK(_Generic(selected, long (*)(int, int, double (*)[4]): 0, default: 1));
  {
    int saved_calls = calls;
    long vla_param_sum(int n, int m, int a[n][m]);
    int local_prototype(int a[capture(rows)]);
    CHECK(vla_param_sum(rows, cols, matrix) == 150);
    CHECK(calls == saved_calls);
  }
#ifdef TEST_VLA_PARAMS_HOST
  CHECK(host_vla_param_sum(rows, cols, matrix) == 150);
  CHECK(host_vla_param_check());
#endif
  puts("VLA parameters OK");
  return 0;
}
