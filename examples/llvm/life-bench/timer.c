#define _POSIX_C_SOURCE 200809L
#include "api.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int argument(char *text, unsigned long maximum, unsigned long *value) {
  char *end;
  if (!*text) return 0;
  for (char *p = text; *p; ++p) if (*p < '0' || *p > '9') return 0;
  errno = 0;
  *value = strtoul(text, &end, 10);
  return !errno && end != text && !*end && *value <= maximum;
}
static double seconds(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
}

int main(int argc, char **argv) {
  unsigned long mode, rounds, seed, steps;
  if (argc != 5 || !argument(argv[1], 5, &mode) ||
      !argument(argv[2], LIFE_MAX_ROUNDS, &rounds) || !rounds ||
      !argument(argv[3], 0xffffffffUL, &seed) || !argument(argv[4], 256, &steps)) {
    fprintf(stderr, "usage: %s MODE(0..5) ROUNDS(1..%d) SEED(0..4294967295) STEPS(0..256)\n",
            argv[0], LIFE_MAX_ROUNDS);
    return 2;
  }
  void *context = life_prepare(mode, seed);
  if (!context) return 3;
  unsigned width = life_width(context), height = life_height(context), population;
  unsigned long fingerprint = life_fingerprint(context), warmup = 0, warmup_population = 0;
  for (unsigned i = 0; i < LIFE_WARMUP; ++i) {
    warmup += life_run(context, i, steps, &population);
    warmup_population += population;
  }
  struct timespec wall_start, wall_end, cpu_start, cpu_end;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) || clock_gettime(CLOCK_MONOTONIC, &wall_start)) return 4;
  unsigned long checksum = 0, population_sum = 0;
  for (unsigned long i = 0; i < rounds; ++i) {
    checksum += life_run(context, i & 7, steps, &population);
    population_sum += population;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &wall_end) || clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end)) return 4;
  life_release(context);
  double wall = seconds(wall_start, wall_end), cpu = seconds(cpu_start, cpu_end);
  if (wall <= 0 || cpu <= 0) return 4;
  printf("{\"mode\":%lu,\"rounds\":%lu,\"seed\":%lu,\"steps\":%lu,\"width\":%u,\"height\":%u,"
         "\"calls\":%lu,\"warmup_rounds\":%d,\"checksum\":%lu,\"population_sum\":%lu,"
         "\"warmup_checksum\":%lu,\"warmup_population\":%lu,\"dataset_fingerprint\":%lu,"
         "\"wall_seconds\":%.9f,\"cpu_seconds\":%.9f}\n",
         mode, rounds, seed, steps, width, height, rounds * steps * width * height,
         LIFE_WARMUP, checksum, population_sum, warmup, warmup_population, fingerprint, wall, cpu);
  return 0;
}
