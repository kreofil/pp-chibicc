int puts(const char *s);

long factorial(int n) {
  if (n <= 1)
    return 1;
  return n * factorial(n - 1);
}

long add_eight(long a, long b, long c, long d,
               long e, long f, long g, long h) {
  return a + b + c + d + e + f + g + h;
}

int main(void) {
  if (factorial(6) != 720 || add_eight(1, 2, 3, 4, 5, 6, 7, 8) != 36)
    return 1;
  puts("functions OK");
  return 7;
}
