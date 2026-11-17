#include "../../ppc-closed-multimethod.h"
static int multi_late<MultiLeft.one *a, MultiRight.one *b>(int x) { return 900 + x; }
#ifndef OMIT_REMOTE_DEFAULT
int multi_remote<MultiLeft *a, MultiRight *b>(int x) { return 600 + x; }
#endif
