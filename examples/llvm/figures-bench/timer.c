#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "api.h"

enum { WARMUP_ROUNDS = 2, MAX_ROUNDS = 10000 };

static int compare_values(const void *left, const void *right) {
  unsigned long a = *(const unsigned long *)left;
  unsigned long b = *(const unsigned long *)right;
  return (a > b) - (a < b);
}

static double seconds(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec) +
         (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
}

int main(int argc, char **argv) {
  if (argc != 3) return 2;
  char *end;
  errno = 0;
  long rounds = strtol(argv[2], &end, 10);
  if (errno || end == argv[2] || *end || rounds < 1 || rounds > MAX_ROUNDS) return 2;
  FILE *input = fopen(argv[1], "r");
  if (!input) return 2;
  int count;
  if (fscanf(input, "%d", &count) != 1 || count < 0 || count > 32768) {
    fclose(input);
    return 2;
  }
  if (!figures_prepare(count)) { fclose(input); figures_destroy(); return 3; }
  unsigned long *reference = count ? malloc(count * sizeof(*reference)) : NULL;
  if (count && !reference) { fclose(input); figures_destroy(); return 3; }
  int correct_input = 1;
  for (int i = 0; i < count; i++) {
    int kind, a, b, c;
    if (fscanf(input, "%d %d %d %d", &kind, &a, &b, &c) != 4 ||
        a < 1 || a > 2000 || b < 1 || b > 2000 || c < 0 || c > 2000 ||
        (kind != 1 && kind != 2) || (kind == 1 && c != 0) ||
        (kind == 2 && (a + b <= c || a + c <= b || b + c <= a))) {
      correct_input = 0;
      break;
    }
    reference[i] = kind == 1 ? 2UL * (a + b) : (unsigned long)(a + b + c);
    if (!figures_add(kind, a, b, c)) { correct_input = 0; break; }
  }
  char extra;
  if (correct_input && fscanf(input, " %c", &extra) == 1) correct_input = 0;
  fclose(input);
  if (!correct_input) { free(reference); figures_destroy(); return 2; }
  if (count > 1) qsort(reference, count, sizeof(*reference), compare_values);
  unsigned long per_round = 0;
  for (int i = 0; i < count; i++) per_round += reference[i] * (i + 1);
  free(reference);

  unsigned long warmup = 0;
  for (int i = 0; i < WARMUP_ROUNDS; i++) warmup += figures_process();
  if (warmup != per_round * WARMUP_ROUNDS || !figures_valid()) { figures_destroy(); return 4; }
  struct timespec wall_start, wall_end, cpu_start, cpu_end;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) ||
      clock_gettime(CLOCK_MONOTONIC, &wall_start)) { figures_destroy(); return 5; }
  unsigned long checksum = 0;
  for (long i = 0; i < rounds; i++) checksum += figures_process();
  if (clock_gettime(CLOCK_MONOTONIC, &wall_end) ||
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end)) { figures_destroy(); return 5; }
  int valid = checksum == per_round * rounds && figures_valid();
  figures_destroy();
  if (!valid) return 4;
  double wall = seconds(wall_start, wall_end);
  double cpu = seconds(cpu_start, cpu_end);
  if (wall <= 0.0 || cpu <= 0.0) return 5;
  printf("{\"size\":%d,\"rounds\":%ld,\"items\":%ld,\"warmup_rounds\":%d,"
         "\"wall_seconds\":%.9f,\"cpu_seconds\":%.9f,\"checksum\":%lu,"
         "\"per_round_checksum\":%lu,\"warmup_checksum\":%lu}\n",
         count, rounds, count * rounds, WARMUP_ROUNDS, wall, cpu,
         checksum, per_round, warmup);
  return 0;
}
