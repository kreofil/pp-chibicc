int puts(const char *s);

int main(void) {
  int sum = 0;
  for (int i = 0; i < 7; i++) {
    if (i == 2)
      continue;
    if (i == 5)
      break;
    sum += i;
  }
  if (sum != 8)
    return 1;
  while (sum < 11)
    sum++;
  do {
    sum--;
  } while (sum > 9);
  if (sum != 9)
    return 2;

  int x = 0;
  int y = 0;
  if ((0 && ++x) || !(1 || ++y) || x != 0 || y != 0)
    return 3;
  int result = x ? ++x : ++y;
  result += y ? ++y : ++x;
  if (result != 3 || x != 0 || y != 2)
    return 4;
  if (!(++x && ++y) || x != 1 || y != 3)
    return 5;
  puts("control OK");
  return 0;
}
