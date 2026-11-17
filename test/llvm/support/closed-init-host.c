int emitted_closed_data_ready(void);
static int ready;
// After tag registration (101), before global-object initialization (401).
static void __attribute__((constructor(102))) check_closed_data(void) {
  ready = emitted_closed_data_ready();
}
int host_closed_data_ready(void) { return ready; }
