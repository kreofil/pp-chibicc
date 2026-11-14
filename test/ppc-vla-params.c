#include <stdarg.h>
int puts(const char *s);
int printf(const char *fmt, ...);
#define CHECK(expr) do { if (!(expr)) { printf("PPC VLA parameter failure: %d\n", __LINE__); return 1; } } while (0)

typedef struct VLAParamShape { int cols; } enum { twice } const VLAParamShape;
typedef struct VLAParamOpen { int cols; } enum { first } VLAParamOpen;
typedef struct VLAParamToken { int value; } enum { marked } const VLAParamToken;
typedef VLAParamShape *VLAParamShapePointer;

static int bound_calls;
static int capture(int n) { ++bound_calls; return n; }

// Declarations use their own parameter names. Runtime bounds belong to each
// actual handler, including references to its polymorphic parameters.
long vla_matrix<VLAParamShape *s>(int height, int width, int a[height][width]);
long vla_matrix<VLAParamShape *shape>(int rows, int cols,
                                    int matrix[capture(rows)][capture(shape->cols)]) {
  int width = shape->cols;
  cols = 1;
  long result = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < width; ++c) result += matrix[r][c];
  return sizeof(*matrix) == width * sizeof(int) ? result : -1000;
}
long vla_matrix<VLAParamShape.twice *shape>(int rows, int cols, int matrix[rows][shape->cols]) {
  long result = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < shape->cols; ++c) result += matrix[r][c];
  return 2 * result;
}

long vla_pair<VLAParamShapePointer a, VLAParamShapePointer b>(int rows, int matrix[rows][a->cols]) {
  return vla_matrix<a>(rows, a->cols, matrix) + b->cols;
}
long vla_pair<VLAParamShape.twice *a, VLAParamShape.twice *b>(int rows,
                                                          int matrix[rows][a->cols]) {
  return vla_matrix<a>(rows, a->cols, matrix) + 100 + b->cols;
}

long vla_open<VLAParamOpen *shape>(int rows, int matrix[rows][shape->cols]) {
  long result = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < shape->cols; ++c) result += matrix[r][c];
  return result;
}

long vla_tokens<VLAParamShape *shape>(int n, VLAParamToken tokens[n]) {
  long result = 0;
  for (int i = 0; i < n; ++i)
    result += tokens[i].value + 10 * get_spec_var_index(&tokens[i]);
  return result;
}

long vla_extra<VLAParamShape *shape>(int rows, int matrix[rows][shape->cols], int n, ...) {
  va_list ap;
  va_start(ap, n);
  long result = vla_matrix<shape>(rows, shape->cols, matrix);
  for (int i = 0; i < n; ++i) result += va_arg(ap, long);
  va_end(ap);
  return result;
}

// Bounds can call another polymorphic function. Its snapshots are locals,
// and the call executes at entry of the chosen handler only.
int vla_columns<VLAParamShape *shape>() { ++bound_calls; return shape->cols; }
long vla_bound_call<VLAParamShape *shape>(int rows, int matrix[rows][vla_columns<shape>()]) {
  return vla_matrix<shape>(rows, shape->cols, matrix);
}

int main(void) {
  int rows = 2, cols = 3;
  int matrix[rows][cols];
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) matrix[r][c] = r * 10 + c + 1;
  VLAParamShape zero = {cols};
  VLAParamShape.twice twice = {cols};
  VLAParamOpen.first opened = {cols};
  CHECK(bound_calls == 0);
  int argument_calls = 0;
  CHECK(vla_matrix<(argument_calls++, &zero)>(rows, cols, matrix) == 42);
  CHECK(argument_calls == 1 && bound_calls == 2);
  bound_calls = 0;
  CHECK(vla_matrix<&twice>(rows, 999, matrix) == 84 && bound_calls == 0);
  CHECK((vla_pair<&zero, &twice>(rows, matrix) == 45));
  CHECK((vla_pair<&twice, &twice>(rows, matrix) == 187));
  CHECK(vla_open<&opened>(rows, matrix) == 42);
  int n = 4;
  VLAParamToken tokens[n];
  for (int i = 0; i < n; ++i) tokens[i].value = i + 1;
  tokens[2] = VLAParamToken.marked;
  CHECK(vla_tokens<&zero>(n, tokens) == 20);
  CHECK(get_spec_var_index(&tokens[2]) == 1 && get_spec_var_index(&tokens[0]) == 0);
  CHECK(vla_extra<&zero>(rows, matrix, 3, 5L, 7L, 11L) == 65);
  bound_calls = 0;
  CHECK(vla_bound_call<&twice>(rows, matrix) == 84 && bound_calls == 1);
  puts("PPC VLA parameters OK");
  return 0;
}
