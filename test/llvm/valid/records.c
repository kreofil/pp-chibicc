#include "../support/record.h"
int puts(const char *s);

int target = 17;
struct Record global_record = {'A', {-3, {5, 9}}, 100, &target};
int *global_item = &global_record.inner.items[1];
struct Packed { char tag; long number; } __attribute__((packed));

long emitted_record_value(struct Record *p) {
  return p->total + p->inner.items[1];
}

int main(void) {
  struct Record local = global_record;
  struct Record copies[2] = {local, global_record};
  local.inner.items[1] = 12;
  if (global_record.inner.items[1] != 9 || copies[1].inner.label != -3)
    return 1;
  local = local;
  struct Record selected = local.total == 100 ? local : global_record;
  if (selected.inner.items[1] != 12 || *selected.pointer != 17 || *global_item != 9)
    return 2;
  _Alignas(16) struct Record aligned;
  aligned = selected;
  if ((unsigned long)&aligned % 16 || emitted_record_value(&aligned) != 112)
    return 3;
  struct Packed packed = {'P', 5000000000L};
  if (sizeof(packed) != 9 || packed.number != 5000000000L)
    return 4;
  union Value { long number; struct { int x, y; } pair; } first, second;
  first.pair.x = 7;
  first.pair.y = 11;
  second = first;
  if (second.pair.x != 7 || second.pair.y != 11)
    return 5;
  if (!host_check_record(&global_record) || global_record.inner.items[0] != 42)
    return 6;
  puts("records OK");
  return 0;
}
