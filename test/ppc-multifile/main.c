#include "test.h"
#include "spec.h"
#include "spec2.h"

int MFForwardUse(MFBase *x);
int MFPointerForwardUse(MFPointer *x);

static MFEnum global_enum_features[2] = {MFEnum.MFMonday, MFEnum.MFThursday};

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

  MFPointer.rect pr = {2, &r.rect};
  MFPointer.trian pt = {3, &t.trian};
  MFPointer values[3] = {pr, pt};
  ASSERT(sizeof(MFPointer), sizeof(MFPointer.rect));
  ASSERT(sizeof(MFPointer), sizeof(MFPointer.trian));
  ASSERT(3, get_spec_size(MFPointer));
  ASSERT(49, MFPointerUse(&values[0]));
  ASSERT(126, MFPointerUse(&values[1]));
  ASSERT(0, MFPointerUse(&values[2]));
  ASSERT(49, MFPointerCopy(&values[2], &values[0]));
  ASSERT(126, MFPointerCopy(&values[0], &values[1]));
  ASSERT(126, MFPointerForwardUse(&values[0]));
  ASSERT(49, MFPointerForwardUse(&values[2]));

  ASSERT(10, MFEnumValue<&global_enum_features[0]>());
  ASSERT(20, MFEnumValue<&global_enum_features[1]>());
  MFEnum e = MFEnum.MFMonday;
  e.seed = 7;
  ASSERT(17, MFEnumValue<&e>());
  MFEnumSetThursday(&e);
  ASSERT(27, MFEnumValue<&e>());
  ASSERT(7, e.seed);
  MFEnumSetMonday(&e);
  ASSERT(17, MFEnumValue<&e>());
  e = MFEnum.MFThursday;
  ASSERT(27, MFEnumValue<&e>());
  printf("OK\n");
  return 0;
}
