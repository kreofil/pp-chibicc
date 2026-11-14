#include "record.h"

int host_check_record(struct Record *p) {
  if (sizeof(*p) != 32 || p->tag != 'A' || p->inner.label != -3 ||
      p->inner.items[0] != 5 || p->inner.items[1] != 9 ||
      *p->pointer != 17 || emitted_record_value(p) != 109)
    return 0;
  p->inner.items[0] = 42;
  return 1;
}
