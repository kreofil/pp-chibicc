int puts(const char *message);

int sum_to(int n) {
  int sum = 0;
  for (int i = 1; i <= n; i++)
    sum += i;
  return sum;
}

int main(void) {
  if (sum_to(10) != 55)
    return 1;
  puts("Hello from ppchibicc LLVM IR");
  return 0;
}
