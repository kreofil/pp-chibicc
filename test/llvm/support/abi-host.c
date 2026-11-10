signed char emitted_char(signed char x);
unsigned char emitted_uchar(unsigned char x);
short emitted_short(short x);
unsigned short emitted_ushort(unsigned short x);
long emitted_long(long x);
int *emitted_pointer(int *p);

signed char host_char(signed char x) { return x - 1; }
unsigned char host_uchar(unsigned char x) { return x + 1; }
short host_short(short x) { return x - 1; }
unsigned short host_ushort(unsigned short x) { return x + 1; }
long host_long(long x) { return x + 2; }
int *host_pointer(int *p) { return p; }

int host_calls_emitted(void) {
  int x = 42;
  return emitted_char(-9) == -10 && emitted_uchar(240) == 241 &&
         emitted_short(-300) == -301 && emitted_ushort(65000) == 65001 &&
         emitted_long(5000000000L) == 5000000002L && emitted_pointer(&x) == &x;
}
