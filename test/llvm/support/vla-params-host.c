long vla_param_sum(int, int, int (*)[*]);

long host_vla_param_sum(int rows, int cols, int matrix[rows][cols]) {
  long result = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) result += matrix[r][c];
  return result;
}

int host_vla_param_check(void) {
  int rows = 2, cols = 3;
  int matrix[rows][cols];
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) matrix[r][c] = r * 10 + c + 1;
  long (*callback)(int, int, int (*)[*]) = vla_param_sum;
  return callback(rows, cols, matrix) == 42;
}
