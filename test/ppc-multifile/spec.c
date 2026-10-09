#include "spec.h"

int MFValue<MFBase.rect *r>() {
  return r->rect.x * 10 + r->rect.y;
}
