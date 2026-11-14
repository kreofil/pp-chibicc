#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Ordinary C harness: the same host-compiled object is used for all backends.
// PPC objects cross this boundary through opaque pointers and scalar values.
typedef struct Shape Shape;
Shape *benchmark_prepare(int mode, Shape **shapes);
long benchmark_visits(Shape *shape);
double dispatch_many(Shape **shapes, int count);

enum { BATCH_COUNT = 4096, WARMUP_ROUNDS = 8, MAX_ROUNDS = 1000000 };

static int argument(const char *text, long low, long high, long *value) {
  char *end;
  errno = 0;
  *value = strtol(text, &end, 10);
  return !errno && end != text && !*end && *value >= low && *value <= high;
}

static double seconds(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec) +
         (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
}

static double expected(int mode, long rounds, long previous_rounds) {
  double calls = (double)rounds * BATCH_COUNT;
  if (mode == 0) return calls * 10.0;
  if (mode == 1) return calls * 9.0;
  if (mode == 2) return calls * 9.5;
  double growing = calls / 2.0;
  double before = (double)previous_rounds * (BATCH_COUNT / 2);
  // Growing starts at 2 and adds 0.5 on each call. Every other call is
  // rectangle (10). The range/step keep these checksums exactly representable.
  return 12.0 * growing + 0.25 * growing * (2.0 * before + growing + 1.0);
}

int main(int argc, char **argv) {
  long mode, rounds;
  if (argc != 3 || !argument(argv[1], 0, 3, &mode) ||
      !argument(argv[2], 1, MAX_ROUNDS, &rounds)) {
    fprintf(stderr, "usage: %s MODE(0..3) ROUNDS(1..%d)\n", argv[0], MAX_ROUNDS);
    return 2;
  }
  Shape *shapes[2];
  Shape *extra = benchmark_prepare((int)mode, shapes);
  if (!shapes[0] || !shapes[1] || (mode == 3 && !extra)) return 3;

  double warmup = 0.0;
  for (int i = 0; i < WARMUP_ROUNDS; i++)
    warmup += dispatch_many(shapes, BATCH_COUNT);
  if (warmup != expected((int)mode, WARMUP_ROUNDS, 0)) return 4;

  struct timespec wall_start, wall_end, cpu_start, cpu_end;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) ||
      clock_gettime(CLOCK_MONOTONIC, &wall_start)) return 5;
  double checksum = 0.0;
  for (long i = 0; i < rounds; i++)
    checksum += dispatch_many(shapes, BATCH_COUNT);
  if (clock_gettime(CLOCK_MONOTONIC, &wall_end) ||
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end)) return 5;

  long visits = benchmark_visits(extra);
  long expected_visits = mode == 3 ? (rounds + WARMUP_ROUNDS) * (BATCH_COUNT / 2) : 0;
  if (checksum != expected((int)mode, rounds, WARMUP_ROUNDS) ||
      visits != expected_visits) {
    fprintf(stderr, "incorrect result: checksum=%.17g visits=%ld\n", checksum, visits);
    return 4;
  }
  double wall = seconds(wall_start, wall_end);
  double cpu = seconds(cpu_start, cpu_end);
  if (wall <= 0.0 || cpu <= 0.0) return 5;
  printf("{\"mode\":%ld,\"rounds\":%ld,\"calls\":%ld,\"warmup_rounds\":%d,"
         "\"wall_seconds\":%.9f,\"cpu_seconds\":%.9f,\"checksum\":%.17g,"
         "\"warmup_checksum\":%.17g,\"visits\":%ld}\n",
         mode, rounds, rounds * BATCH_COUNT, WARMUP_ROUNDS,
         wall, cpu, checksum, warmup, visits);
  return 0;
}
