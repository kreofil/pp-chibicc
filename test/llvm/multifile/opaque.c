// Deliberately compile without the closed definition. The shared count must
// still be available to opaque users and the runtime table in this module.
struct MFClosedChoice;
int closed_pick<struct MFClosedChoice *x>(void) = 0;
int closed_opaque(struct MFClosedChoice *x) {
  if (get_spec_size(struct MFClosedChoice) != 3) return -1;
  return closed_pick<x>();
}
