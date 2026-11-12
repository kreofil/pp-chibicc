#include "api.h"
int registry_combine<RegistryLeft *a, RegistryRight *b>(int x) {
  return 100 + 10 * get_spec_var_index(a) + get_spec_var_index(b) + x;
}
