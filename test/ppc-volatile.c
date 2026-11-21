#include <stdio.h>
#define CHECK(x) do { if (!(x)) { printf("PPC volatile failure: %d\n", __LINE__); return 1; } } while (0)
typedef struct Registers { volatile int value; volatile unsigned bits:3; } Registers;
typedef struct Device { volatile int status; } case { Registers registers; } const Device;
int read_device<volatile Device *d>() { return d->status; }
int read_device<volatile Device.registers *d>() { return d->status+d->registers.value; }
int update_device<volatile Device *d>(int n) { return d->status+=n; }
int update_device<volatile Device.registers *d>(int n) {
  d->registers.bits++; return d->registers.value+=n;
}
int read_pair<volatile Device *a, volatile Device *b>() { return a->status+b->status; }
int read_pair<volatile Device.registers *a, volatile Device.registers *b>() {
  return a->registers.value+b->registers.value;
}
int main(void) {
  volatile Device base={3};
  volatile Device.registers item={5,{7,6}};
  CHECK(read_device<&base>()==3 && read_device<&item>()==12);
  CHECK(update_device<&base>(2)==5 && update_device<&item>(4)==11);
  CHECK(item.registers.bits==7);
  CHECK((read_pair<&base,&item>()==10));
  CHECK((read_pair<&item,&item>()==22));
  int calls=0; CHECK(read_device<(calls++,&item)>()==16 && calls==1);
  puts("PPC volatile OK");
  return 0;
}
