#ifndef PPC_LIFE_BENCH_API_H
#define PPC_LIFE_BENCH_API_H
#define LIFE_MAX_ROUNDS 4096
#define LIFE_WARMUP 8
void *life_prepare(unsigned mode, unsigned seed);
void life_release(void *context);
unsigned long life_run(void *context, unsigned phase, unsigned steps, unsigned *population);
unsigned long life_fingerprint(void *context);
unsigned life_width(void *context);
unsigned life_height(void *context);
#endif
