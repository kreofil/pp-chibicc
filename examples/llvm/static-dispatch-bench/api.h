#ifndef STATIC_DISPATCH_BENCH_API_H
#define STATIC_DISPATCH_BENCH_API_H
enum { BENCH_BATCH = 4096, BENCH_WARMUP = 8, BENCH_MAX_ROUNDS = 1000000 };
typedef struct { unsigned a, b, c; unsigned long x; } BenchRecord;
typedef struct { unsigned long concrete, defaults; } BenchStats;
void *bench_prepare(const BenchRecord *records, int count);
void bench_release(void *context);
unsigned long bench_run(void *context, int arity, int work, BenchStats *stats);
#endif
