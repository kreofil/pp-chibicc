int puts(const char *s);
signed char host_char(signed char x);
unsigned char host_uchar(unsigned char x);
short host_short(short x);
unsigned short host_ushort(unsigned short x);
long host_long(long x);
int *host_pointer(int *p);
int host_calls_emitted(void);

signed char emitted_char(signed char x) { return x - 1; }
unsigned char emitted_uchar(unsigned char x) { return x + 1; }
short emitted_short(short x) { return x - 1; }
unsigned short emitted_ushort(unsigned short x) { return x + 1; }
long emitted_long(long x) { return x + 2; }
int *emitted_pointer(int *p) { return p; }

int main(void) {
  int value = 42;
  if (host_char(-9) != -10 || host_uchar(240) != 241)
    return 1;
  if (host_short(-300) != -301 || host_ushort(65000) != 65001)
    return 2;
  if (host_long(5000000000L) != 5000000002L || *host_pointer(&value) != 42)
    return 3;
  if (!host_calls_emitted())
    return 4;
  puts("ABI OK");
  return 0;
}
