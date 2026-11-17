#include "families.h"
#include <stdlib.h>
static BenchLeft left[2];
static BenchRight right[3];
static BenchThird third[2];

void *bench_prepare(const BenchRecord *records, int count) {
  if (count < 0 || count > BENCH_BATCH) return 0;
  BenchContext *context = malloc(sizeof(BenchContext));
  if (!context) return 0;
  context->jobs = malloc(sizeof(BenchJob) * (count ? count : 1));
  if (!context->jobs) { free(context); return 0; }
  context->count = count;
  left[1] = BenchLeft.one;
  right[1] = BenchRight.one;
  right[2] = BenchRight.two;
  third[1] = BenchThird.one;
  for (int i = 0; i < count; i++) {
    if (records[i].a >= 2 || records[i].b >= 3 || records[i].c >= 2) {
      bench_release(context); return 0;
    }
    context->jobs[i].a = &left[records[i].a];
    context->jobs[i].b = &right[records[i].b];
    context->jobs[i].c = &third[records[i].c];
    context->jobs[i].x = records[i].x;
  }
  return context;
}

void bench_release(void *pointer) {
  BenchContext *context = pointer;
  if (context) { free(context->jobs); free(context); }
}
