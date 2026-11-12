#include "../../ppc-closed-dispatch.h"
// A distinct, private handler may register into the same public family.
static int dispatch_pick<DispatchChoice.second *p>(int x) { return x + 300; }
int host_active_second(void) {
  DispatchChoice p = DispatchChoice.second;
  return dispatch_pick<&p>(0);
}

int dispatch_external<DispatchChoice.second *p>(void) { return 42; }
