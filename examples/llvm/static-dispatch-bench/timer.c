#define _POSIX_C_SOURCE 200809L
#include "api.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int timer_argument(char *text, unsigned long maximum, unsigned long *value) {
  char *end;
  errno = 0;
  *value = strtoul(text, &end, 10);
  return !errno && end != text && !*end && *value <= maximum;
}
static unsigned timer_next(unsigned *state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}
static unsigned long timer_dataset(BenchRecord *records, int count, int arity, int hot, unsigned seed) {
  unsigned state = seed;
  int cells = arity == 2 ? 6 : 12;
  int dominant = count * 9 / 10;
  for (int i = 0; i < count; i++) {
    unsigned flat = hot ? (i < dominant ? cells - 1 : (i - dominant) % (cells - 1)) : i % cells;
    records[i].a = arity == 2 ? flat / 3 : flat / 6;
    records[i].b = arity == 2 ? flat % 3 : flat / 2 % 3;
    records[i].c = arity == 2 ? 0 : flat % 2;
    records[i].x = timer_next(&state) >> 16;
  }
  for (int i = count - 1; i > 0; i--) {
    unsigned j = timer_next(&state) % (unsigned)(i + 1);
    BenchRecord temporary = records[i]; records[i] = records[j]; records[j] = temporary;
  }
  unsigned long fingerprint = 14695981039346656037UL;
  for (int i = 0; i < count; i++) {
    fingerprint = (fingerprint ^ records[i].a) * 1099511628211UL;
    fingerprint = (fingerprint ^ records[i].b) * 1099511628211UL;
    fingerprint = (fingerprint ^ records[i].c) * 1099511628211UL;
    fingerprint = (fingerprint ^ records[i].x) * 1099511628211UL;
  }
  return fingerprint;
}
static unsigned long timer_reference(BenchRecord *records, int count, int arity, int work, unsigned long *concrete) {
  unsigned long sum = 0;
  *concrete = 0;
  for (int i = 0; i < count; i++) {
    unsigned flat = arity == 2 ? records[i].a * 3 + records[i].b : records[i].a * 6 + records[i].b * 2 + records[i].c;
    int specific = flat == 0 || flat == (arity == 2 ? 5 : 11);
    *concrete += specific;
    unsigned factor = flat + (specific ? 101 : 37);
    unsigned long value = (records[i].x + 1) * factor + 13;
    if (work) {
      value = records[i].x + factor;
      for (unsigned step = 0; step != 6; ++step) {
        value = (value ^ (value >> 13)) * 0x9e3779b185ebca87UL;
        value = (value ^ (value >> 17)) + factor + step;
      }
      value &= 0xffffffUL;
    }
    sum += value;
  }
  return sum;
}
static double timer_seconds(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
}

int main(int argc, char **argv) {
  unsigned long mode, rounds, seed, count;
  if (argc != 5 || !timer_argument(argv[1], 7, &mode) ||
      !timer_argument(argv[2], BENCH_MAX_ROUNDS, &rounds) || !rounds ||
      !timer_argument(argv[3], 0xffffffffUL, &seed) || !timer_argument(argv[4], BENCH_BATCH, &count)) {
    fprintf(stderr, "usage: %s MODE(0..7) ROUNDS(1..%d) SEED(0..4294967295) COUNT(0..%d)\n",
            argv[0], BENCH_MAX_ROUNDS, BENCH_BATCH);
    return 2;
  }
  int arity = mode < 4 ? 2 : 3;
  int work = (mode / 2) % 2;
  BenchRecord records[BENCH_BATCH];
  unsigned long fingerprint = timer_dataset(records, count, arity, mode % 2, seed);
  unsigned long specific, per_round = timer_reference(records, count, arity, work, &specific);
  void *context = bench_prepare(records, count);
  if (!context) return 3;
  BenchStats stats = {0};
  unsigned long warmup = 0;
  for (int i = 0; i < BENCH_WARMUP; i++) warmup += bench_run(context, arity, work, &stats);
  if (warmup != per_round * BENCH_WARMUP) return 4;
  struct timespec wall_start, wall_end, cpu_start, cpu_end;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) || clock_gettime(CLOCK_MONOTONIC, &wall_start)) return 5;
  unsigned long checksum = 0;
  for (unsigned long i = 0; i < rounds; i++) checksum += bench_run(context, arity, work, &stats);
  if (clock_gettime(CLOCK_MONOTONIC, &wall_end) || clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end)) return 5;
  bench_release(context);
  if (checksum != per_round * rounds || stats.concrete != specific * (rounds + BENCH_WARMUP) ||
      stats.defaults != (count - specific) * (rounds + BENCH_WARMUP)) return 4;
  double wall = timer_seconds(wall_start, wall_end), cpu = timer_seconds(cpu_start, cpu_end);
  if (wall <= 0 || cpu <= 0) return 5;
  printf("{\"mode\":%lu,\"rounds\":%lu,\"seed\":%lu,\"count\":%lu,\"calls\":%lu,\"warmup_rounds\":%d,"
         "\"wall_seconds\":%.9f,\"cpu_seconds\":%.9f,\"checksum\":%lu,\"warmup_checksum\":%lu,"
         "\"concrete\":%lu,\"defaults\":%lu,\"dataset_fingerprint\":%lu}\n",
         mode, rounds, seed, count, rounds * count, BENCH_WARMUP, wall, cpu, checksum, warmup,
         stats.concrete, stats.defaults, fingerprint);
  return 0;
}
