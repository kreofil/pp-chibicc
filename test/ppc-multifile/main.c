#include "test.h"
#include "spec.h"
#include "spec2.h"

int MFForwardUse(MFBase *x);

int main(void) {
  MFBase.rect r;
  r.rect.x = 4;
  r.rect.y = 7;

  MFBase.trian t;
  t.trian.a = 1;
  t.trian.b = 2;
  t.trian.c = 3;

  ASSERT(47, MFUse((MFBase *)&r));
  ASSERT(123, MFUse((MFBase *)&t));
  ASSERT(47, MFForwardUse((MFBase *)&r));
  ASSERT(123, MFForwardUse((MFBase *)&t));
  printf("OK\n");
  return 0;
}
