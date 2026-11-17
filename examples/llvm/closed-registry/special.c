#include "api.h"
int registry_combine<RegistryLeft.void *a, RegistryRight.void *b>(int x) { return 200 + x; }
int registry_combine<RegistryLeft.one *a, RegistryRight.two *b>(int x) { return 500 + x; }
