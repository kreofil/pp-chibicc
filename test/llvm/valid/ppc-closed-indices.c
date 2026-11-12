#define main closed_indices_main
#include "../../ppc-closed-indices.c"
#undef main
int host_closed_data_ready(void);
int emitted_closed_data_ready(void) {
  return get_spec_var_index(&explicit_case) == 2 &&
         get_spec_var_index(&global_features[0]) == 2 &&
         get_spec_var_index(&global_features[1]) == 1 &&
         get_spec_var_index(&global_pointer) == 2 &&
         get_spec_var_index(&global_union) == 2 &&
         get_spec_var_index(&nested_closed) == 2 &&
         get_spec_var_index(&nested_closed.closed) == 2;
}
int main(void) {
  if (!host_closed_data_ready()) return 3;
  return closed_indices_main();
}
