int puts(const char *s);

int main(void) {
  int values[4] = {3, 5, 7, 9};
  int *p = values;
  int **pp = &p;
  *(*pp + 1) = 11;
  if (values[1] != 11 || p[2] != 7)
    return 1;
  if (&values[3] - &values[0] != 3)
    return 2;
  int sum = 0;
  for (int i = 0; i < 4; i++)
    sum += *(p + i);
  if (sum != 30 || !p || (_Bool)p != 1)
    return 3;
  puts("pointers OK");
  return 0;
}
