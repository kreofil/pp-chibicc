#include <stdio.h>

typedef struct MatrixView { int cols; } enum { doubled } const MatrixView;

long matrix_sum<MatrixView *view>(int rows, int values[rows][view->cols]) {
  long result = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < view->cols; ++c) result += values[r][c];
  return result;
}

long matrix_sum<MatrixView.doubled *view>(int rows, int values[rows][view->cols]) {
  long result = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < view->cols; ++c) result += values[r][c];
  return 2 * result;
}

int main(void) {
  int rows = 2, cols = 3;
  int values[rows][cols];
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) values[r][c] = r * cols + c + 1;
  MatrixView plain = {cols};
  MatrixView.doubled doubled = {cols};
  long first = matrix_sum<&plain>(rows, values);
  long second = matrix_sum<&doubled>(rows, values);
  printf("matrix: sum=%ld, doubled=%ld\n", first, second);
  return first != 21 || second != 42;
}
