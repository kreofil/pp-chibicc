int puts(const char *s);

signed char negative_char(signed char x) { return x; }
unsigned char positive_char(unsigned char x) { return x; }
short negative_short(short x) { return x; }
unsigned short positive_short(unsigned short x) { return x; }

int main(void) {
  if (negative_char(-7) != -7 || positive_char(250) != 250)
    return 1;
  if (negative_short(-300) != -300 || positive_short(65000) != 65000)
    return 2;
  if ((unsigned int)-1 / 2 != 2147483647U)
    return 3;
  if (-17 / 5 != -3 || -17 % 5 != -2)
    return 4;
  if (((unsigned long)-1 >> 63) != 1 || (-8 >> 2) != -2)
    return 5;
  if ((_Bool)256 != 1 || (_Bool)0 != 0 || (unsigned char)257 != 1)
    return 6;
  if ((~5 & 15) != 10 || (9 ^ 3) != 10 || (2 | 4) != 6)
    return 7;
  // The initial target is LP64 Linux x86-64: integer-to-pointer casts use
  // the source integer's signedness when extending to pointer width.
  if ((long)(void *)(int)-1 != -1L ||
      (long)(void *)(short)-2 != -2L)
    return 8;
  if ((unsigned long)(void *)(unsigned int)-1 != 4294967295UL)
    return 9;
  unsigned char c = 1;
  unsigned short s = 1;
  if ((c >> 8) != 0 || (s >> 16) != 0 || (c << 8) != 256 ||
      (s << 16) != 65536 || ~(unsigned char)0 != -1)
    return 10;
  if (sizeof(~c) != sizeof(int) || sizeof(s << 1) != sizeof(int))
    return 11;
  puts("integers OK");
  return 0;
}
